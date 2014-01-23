/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://mozilla.org/.
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * Copyright (C) 2006 Stephen Deasey <sdeasey@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms
 * of the GNU General Public License (the "GPL"), in which case the
 * provisions of GPL are applicable instead of those above.  If you wish
 * to allow use of your version of this file only under the terms of the
 * GPL and not to allow others to use your version of this file under the
 * License, indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by the GPL.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under either the License or the GPL.
 */

/*
 * nsdbipg.c --
 *
 *      Implements the nsdbi database driver callbacks for Postgres.
 */

#include "nsdbidrv.h"
#include "libpq-fe.h"



NS_EXPORT int Ns_ModuleVersion = 1;


/*
 * The following structure manages per-pool configuration.
 */

typedef struct PGConfig {

    CONST char *module;
    CONST char *datasource;

} PgConfig;

/*
 * The following structure tracks a handle connection
 * and the current result.
 */

typedef struct PgHandle {

    PgConfig  *pgCfg;  /* Pool configuration. */

    PGconn    *conn;   /* Connection to postgres backend. */
    PGresult  *res;    /* Current result. */
    int        rowIdx; /* Current row of result being processed. */

} PgHandle;


/*
 * Static functions defined in this file.
 */

static Dbi_OpenProc         Open;
static Dbi_CloseProc        Close;
static Dbi_ConnectedProc    Connected;
static Dbi_BindVarProc      Bind;
static Dbi_PrepareProc      Prepare;
static Dbi_PrepareCloseProc PrepareClose;
static Dbi_ExecProc         Exec;
static Dbi_NextRowProc      NextRow;
static Dbi_ColumnLengthProc ColumnLength;
static Dbi_ColumnValueProc  ColumnValue;
static Dbi_ColumnNameProc   ColumnName;
static Dbi_TransactionProc  Transaction;
static Dbi_FlushProc        Flush;
static Dbi_ResetProc        Reset;

static void NoticeProcessor(void *arg, const char *message);
static void SetException(Dbi_Handle *handle, PGresult *res);

/*
 * Static variables defined in this file.
 */

static Dbi_DriverProc procs[] = {
    {Dbi_OpenProcId,         (Ns_Callback *) Open},
    {Dbi_CloseProcId,        (Ns_Callback *) Close},
    {Dbi_ConnectedProcId,    (Ns_Callback *) Connected},
    {Dbi_BindVarProcId,      (Ns_Callback *) Bind},
    {Dbi_PrepareProcId,      (Ns_Callback *) Prepare},
    {Dbi_PrepareCloseProcId, (Ns_Callback *) PrepareClose},
    {Dbi_ExecProcId,         (Ns_Callback *) Exec},
    {Dbi_NextRowProcId,      (Ns_Callback *) NextRow},
    {Dbi_ColumnLengthProcId, (Ns_Callback *) ColumnLength},
    {Dbi_ColumnValueProcId,  (Ns_Callback *) ColumnValue},
    {Dbi_ColumnNameProcId,   (Ns_Callback *) ColumnName},
    {Dbi_TransactionProcId,  (Ns_Callback *) Transaction},
    {Dbi_FlushProcId,        (Ns_Callback *) Flush},
    {Dbi_ResetProcId,        (Ns_Callback *) Reset},
    {0, NULL}
};


/*
 *----------------------------------------------------------------------
 *
 * Ns_ModuleInit --
 *
 *      Register the driver functions.
 *
 * Results:
 *      NS_OK.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

NS_EXPORT int
Ns_ModuleInit(CONST char *server, CONST char *module)
{
    PgConfig   *pgCfg;
    char       *path;
    CONST char *drivername = "pg";
    CONST char *database   = "postgres";

    Dbi_LibInit();

    path = Ns_ConfigGetPath(server, module, NULL);

    pgCfg = ns_malloc(sizeof(PgConfig));
    pgCfg->module           = ns_strdup(module);
    pgCfg->datasource       = Ns_ConfigString(path, "datasource", "connect_timeout=30");

    return Dbi_RegisterDriver(server, module,
                              drivername, database,
                              procs, pgCfg);
}


/*
 *----------------------------------------------------------------------
 *
 * Open --
 *
 *      Open a connection to the configured postgres database.
 *
 * Results:
 *      NS_OK or NS_ERROR.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
Open(ClientData configData, Dbi_Handle *handle)
{
    PgConfig *pgCfg = configData;
    PgHandle *pgHandle;
    PGconn   *conn;

    conn = PQconnectdb(pgCfg->datasource);

    if (PQstatus(conn) != CONNECTION_OK) {
        Dbi_SetException(handle, "PGSQL", PQerrorMessage(conn));
        PQfinish(conn);
        return NS_ERROR;
    }

    /*
     * Make sure the database is expecting and returning utf8 character data.
     */

    if (PQsetClientEncoding(conn, "UTF8") != 0) {
        Dbi_SetException(handle, "PGSQL", "failed to set UTF-8 encoding");
        PQfinish(conn);
        return NS_ERROR;
    }

    pgHandle = ns_calloc(1, sizeof(PgHandle));
    pgHandle->pgCfg = pgCfg;
    pgHandle->conn = conn;
    handle->driverData = pgHandle;

    (void) PQsetNoticeProcessor(conn, NoticeProcessor, pgCfg);

    /*
     * Make sure the database is expecting and returning utf8 character data.
     * Set the timezone to UTC and the datestyle to ISO.
     */

    if (Dbi_ExecDirect(handle, "set session client_encoding = 'UTF8'") != NS_OK
        || Dbi_ExecDirect(handle, "set session timezone = 'UTC'") != NS_OK
        || Dbi_ExecDirect(handle, "set session datestyle = 'ISO'") != NS_OK) {

        PQfinish(pgHandle->conn);
        ns_free(pgHandle);
        handle->driverData = NULL;
        return NS_ERROR;
    }

    Dbi_SetException(handle, "00000", "server version: %d protocol version: %d",
                     PQserverVersion(conn), PQprotocolVersion(conn));

    return NS_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * Close --
 *
 *      Close a database connection.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
Close(Dbi_Handle *handle)
{
    PgHandle *pgHandle = handle->driverData;

    if (pgHandle != NULL) {
        if (pgHandle->res != NULL) {
            PQclear(pgHandle->res);
        }
        if (pgHandle->conn != NULL) {
            PQfinish(pgHandle->conn);
        }
        ns_free(pgHandle);
        handle->driverData = NULL;
    }
}


/*
 *----------------------------------------------------------------------
 *
 * Connected --
 *
 *      Is the given handle currently connected?
 *
 * Results:
 *      NS_TRUE if connected, NS_FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
Connected(Dbi_Handle *handle)
{
    PgHandle *pgHandle = handle->driverData;

    if (pgHandle != NULL
            && pgHandle->conn != NULL
            && PQstatus(pgHandle->conn) == CONNECTION_OK) {
        return NS_TRUE;
    }
    return NS_FALSE;
}


/*
 *----------------------------------------------------------------------
 *
 * Bind --
 *
 *      Append a bind variable place holder in Postgres syntax to the
 *      given dstring.
 *
 *      Postgres bind variables start at 1, nsdbi at 0;
 *
 * Results:
 *      NS_OK.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
Bind(Ns_DString *ds, CONST char *name, int bindIdx)
{
    Ns_DStringPrintf(ds, "$%d", bindIdx + 1);
}


/*
 *----------------------------------------------------------------------
 *
 * Prepare --
 *
 *      Prepare a per-handle statement with the postgres backend
 *      using the dbi statement ID as statement name.
 *
 * Results:
 *      NS_OK or NS_ERROR.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
Prepare(Dbi_Handle *handle, Dbi_Statement *stmt,
        unsigned int *numVarsPtr, unsigned int *numColsPtr)
{
    PgHandle    *pgHandle = handle->driverData;

    if (stmt->driverData == NULL) {
	char         stmtName[64];
	PGresult    *res;

        snprintf(stmtName, sizeof(stmtName), "dbipg_%u", stmt->id);

        res = PQprepare(pgHandle->conn,
                        stmtName,
                        stmt->sql,
                        0,         /* nParams (types specified) */
                        NULL);     /* paramTypes (OID) */

        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            SetException(handle, res);
            PQclear(res);

            return NS_ERROR;
        }
        PQclear(res);

        res = PQdescribePrepared(pgHandle->conn, stmtName);
        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            SetException(handle, res);
            PQclear(res);

            return NS_ERROR;
        }

        *numVarsPtr = (unsigned int) PQnparams(res);
        *numColsPtr = (unsigned int) PQnfields(res);

        PQclear(res);

        stmt->driverData = ns_strdup(stmtName);
    }

    return NS_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * PrepareClose --
 *
 *      Cleanup a prepared statement.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
PrepareClose(Dbi_Handle *handle, Dbi_Statement *stmt)
{
    char       *stmtName = stmt->driverData;
    Ns_DString  ds;

    if (stmtName != NULL) {

        if (Connected(handle)) {
            Ns_DStringInit(&ds);
            Ns_DStringPrintf(&ds, "deallocate %s", stmtName);
            (void) Dbi_ExecDirect(handle, Ns_DStringValue(&ds));
            Ns_DStringFree(&ds);
        }
        ns_free(stmtName);
        stmt->driverData = NULL;
    }
}


/*
 *----------------------------------------------------------------------
 *
 * Exec --
 *
 *      Bind values to variables and execute the statement.
 *
 * Results:
 *      NS_OK or NS_ERROR.
 *
 * Side effects:
 *      Memory is allocated on stack for pg params and lengths.
 *
 *----------------------------------------------------------------------
 */

static int
Exec(Dbi_Handle *handle, Dbi_Statement *stmt,
     Dbi_Value *values, unsigned int numValues)
{
    PgHandle   *pgHandle = handle->driverData;
    char       *stmtName = stmt->driverData;
    CONST char *pgValues[DBI_MAX_BIND];
    int         pgLengths[DBI_MAX_BIND];
    int         pgFormats[DBI_MAX_BIND];
    PGresult   *res;
    int         i, rc;

    (void) Flush(handle, stmt);

    for (i = 0; i < numValues; i++) {
        pgValues[i]  = values[i].data;
        pgLengths[i] = values[i].length;
        pgFormats[i] = values[i].binary ? 1 : 0;
    }

    res = PQexecPrepared(pgHandle->conn,
                         stmtName,   /* stmtName */
                         numValues,  /* nParams */
                         pgValues,
                         pgLengths,
                         pgFormats,  /* paramFormats (NULL == text) */
                         0);         /* resultFormat (0 == text) */

    rc = PQresultStatus(res);

    if (rc != PGRES_TUPLES_OK
            && rc != PGRES_COMMAND_OK) {

        SetException(handle, res);
        PQclear(res);
        return NS_ERROR;
    }

    pgHandle->res = res;
    pgHandle->rowIdx = -1;

    return NS_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * NextRow --
 *
 *      Increment the row index.
 *
 * Results:
 *      NS_OK, endPtr set to 1 after last row fetched.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
NextRow(Dbi_Handle *handle, Dbi_Statement *stmt, int *endPtr)
{
    PgHandle *pgHandle = handle->driverData;

    pgHandle->rowIdx++;

    if (pgHandle->rowIdx >= PQntuples(pgHandle->res)) {
        *endPtr = 1;
    }

    return NS_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * ColumnLength --
 *
 *      Return the length of the column value and it's text/binary
 *      type after a NextRow(). Null values are 0 length.
 *
 * Results:
 *      NS_OK;
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
ColumnLength(Dbi_Handle *handle, Dbi_Statement *stmt, unsigned int index,
             size_t *lengthPtr, int *binaryPtr)
{
    PgHandle *pgHandle = handle->driverData;

    if (PQgetisnull(pgHandle->res, pgHandle->rowIdx, (int) index)) {
        *lengthPtr = 0;
    } else {
        *lengthPtr = PQgetlength(pgHandle->res, pgHandle->rowIdx, (int) index);
    }

    /*
     * FIXME: Identify bytea and blob by pg oid and decode to byte
     *        string, or fix Prepare() to ask for bytea values
     *        in a binary format.
     */

    *binaryPtr = 0;

    return NS_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * ColumnValue --
 *
 *      Fetch the indicated value from the current row.
 *
 * Results:
 *      NS_OK.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
ColumnValue(Dbi_Handle *handle, Dbi_Statement *stmt, unsigned int index,
            char *value, size_t maxLength)
{
    PgHandle *pgHandle = handle->driverData;
    char     *src;
    int       srclen;

    src = PQgetvalue(pgHandle->res, pgHandle->rowIdx, (int) index);
    if (src == NULL) {
        Dbi_SetException(handle, "PGSQL",
                         "bad row or column index while fetching value: "
                         "column: %u row: %u",
                         index, pgHandle->rowIdx);
        return NS_ERROR;
    }
    srclen = PQgetlength(pgHandle->res, pgHandle->rowIdx, (int) index);

    memcpy(value, src, MIN(maxLength, srclen));

    return NS_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * ColumnName --
 *
 *      Fetch the name of the specified column index.
 *
 * Results:
 *      NS_OK or NS_ERROR
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
ColumnName(Dbi_Handle *handle, Dbi_Statement *stmt,
           unsigned int index, CONST char **column)
{
    PgHandle *pgHandle = handle->driverData;

    if ((*column = PQfname(pgHandle->res, index)) == NULL) {
        Dbi_SetException(handle, "PGSQL",
            "bug: bad column index while fetching value: column: %u", index);
        return NS_ERROR;
    }

    return NS_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * Transaction --
 *
 *      Begin, commit and rollback transactions.
 *
 * Results:
 *      NS_OK or NS_ERROR
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
Transaction(Dbi_Handle *handle, unsigned int depth,
            Dbi_TransactionCmd cmd, Dbi_Isolation isolation)
{
    PgHandle   *pgHandle = handle->driverData;
    Ns_DString  ds;
    CONST char *sql;

    static CONST char *levels[] = {
        "read uncommitted", /* Dbi_ReadUncommitted */
        "read committed",   /* Dbi_ReadCommitted */
        "repeatable read",  /* Dbi_RepeatableRead */
        "serializable"      /* Dbi_Serializable */
    };

    Ns_DStringInit(&ds);

    switch(cmd) {

    case Dbi_TransactionBegin:
        sql = depth
            ? Ns_DStringPrintf(&ds, "savepoint dbipg_%u", depth)
            : Ns_DStringPrintf(&ds, "begin isolation level %s", levels[isolation]);
        break;

    case Dbi_TransactionCommit:
        sql = depth ? NULL : "commit";
        break;

    case Dbi_TransactionRollback:
        sql = depth
            ? Ns_DStringPrintf(&ds, "rollback to savepoint dbipg_%u", depth)
            : "rollback";
        break;

    default:
        Ns_Fatal("dbipg: Transaction: unhandled cmd: %d", (int) cmd);
    }

    if (sql != NULL && Dbi_ExecDirect(handle, sql) != NS_OK) {
        SetException(handle, pgHandle->res);
        Ns_DStringFree(&ds);
        return NS_ERROR;
    }

    Ns_DStringFree(&ds);

    return NS_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * Flush --
 *
 *      Clear the current result, which discards any pending rows.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
Flush(Dbi_Handle *handle, Dbi_Statement *stmt)
{
    PgHandle *pgHandle = handle->driverData;

    if (pgHandle->res != NULL) {
        PQclear(pgHandle->res);
        pgHandle->res = NULL;
    }
    return NS_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * Reset --
 *
 *      Nothing to do but check that the connection to the db is
 *      still good.
 *
 * Results:
 *      NS_OK if connection OK, NS_ERROR if disconnected.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
Reset(Dbi_Handle *handle)
{
    PgHandle *pgHandle = handle->driverData;

    if (PQstatus(pgHandle->conn) == CONNECTION_BAD) {
        PQfinish(pgHandle->conn);
        pgHandle->conn = NULL;
        return NS_ERROR;
    }
    return NS_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * NoticeProcessor --
 *
 *      Callback to log postgres messages.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
NoticeProcessor(void *arg, const char *message)
{
    PgConfig *pgCfg = arg;

    Ns_Log(Warning, "dbipg[%s]: %s", pgCfg->module, message);
}


/*
 *----------------------------------------------------------------------
 *
 * SetException --
 *
 *      Set the dbi from the given postgres result set.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
SetException(Dbi_Handle *handle, PGresult *res)
{
    Dbi_SetException(handle,
                     PQresultErrorField(res, PG_DIAG_SQLSTATE),
                     PQresultErrorField(res, PG_DIAG_MESSAGE_PRIMARY));
}
