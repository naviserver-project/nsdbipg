#
# nsdbipg configuration example.
#
#     The nsdbipg Postgres database driver takes only one
#     extra configuration parameter: datasource.
#
#     Format is: "key=value key='value'". Values may be single quoted.
#


#
# Global pools.
#
ns_section "ns/modules"
ns_param   pool1          nsdbipg.so


#
# Private pools
#
ns_section "ns/server/server1/modules"
ns_param   pool2         nsdbipg.so


#
# Pool 2 configuration.
#
ns_section "ns/server/server1/module/pool2"
#
# The following are standard nsdbi config options.
# See nsdbi for details.
#
ns_param   default        true ;# This is the default pool for server1.
ns_param   handles        2    ;# Max open handles to db.
ns_param   maxwait        10   ;# Seconds to wait if handle unavailable.
ns_param   maxidle        0    ;# Handle closed after maxidle seconds if unused.
ns_param   maxopen        0    ;# Handle closed after maxopen seconds, regardless of use.
ns_param   maxqueries     0    ;# Handle closed after maxqueries SQL queries.
ns_param   checkinterval  600  ;# Check for idle handles every 10 minutes.
#
# The following is the postgres connection info that specifies
# which database to connect to, username, etc.
#
# See Postgres docs for full details of format and options available.
#
ns_param   datasource     "user=x password=y dbname=mydb"
ns_param   session_timezone UTC
