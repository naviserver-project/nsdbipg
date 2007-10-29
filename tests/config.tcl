#
# nsdbipg configuration test.
#


set homedir   [pwd]
set bindir    [file dirname [ns_info nsd]]



#
# Global Naviserver parameters.
#

ns_section "ns/parameters"
ns_param   home           $homedir
ns_param   tcllibrary     $bindir/../tcl
ns_param   logdebug       false

ns_section "ns/modules"
ns_param   nssock         $bindir/nssock.so

ns_section "ns/module/nssock"
ns_param   port            8080
ns_param   hostname        localhost
ns_param   address         127.0.0.1
ns_param   defaultserver   server1

ns_section "ns/module/nssock/servers"
ns_param   server1         server1

ns_section "ns/servers"
ns_param   server1         "Server One"


#
# Server One configuration.
#

ns_section "ns/server/server1/tcl"
ns_param   initfile        ${bindir}/init.tcl
ns_param   library         $homedir/tests/testserver/modules

ns_section "ns/server/server1/modules"
ns_param   pool1           $homedir/nsdbipg.so
ns_param   pool2           $homedir/nsdbipg.so
ns_param   pool3           $homedir/nsdbipg.so
ns_param   thread          $homedir/nsdbipg.so
ns_param   thread2         $homedir/nsdbipg.so

#
# Database configuration.
#

set user     [ns_env get DBIPG_USER]
set password [ns_env get DBIPG_PASSWORD]
set dbname   [ns_env get DBIPG_DBNAME]

ns_section "ns/server/server1/module/pool1"
ns_param   default         true
ns_param   maxhandles      5
ns_param   maxidle         0
ns_param   maxopen         0
ns_param   maxqueries      1000000
ns_param   checkinterval   30
ns_param   datasource      "user='$user' password='$password' dbname='$dbname' connect_timeout=5"

ns_section "ns/server/server1/module/pool2"
ns_param   maxhandles      1
ns_param   datasource      "invalid data source"

ns_section "ns/server/server1/module/pool3"
ns_param   maxhandles      1
ns_param   datasource      "dbname='does_not_exist' connect_timeout=5"

ns_section "ns/server/server1/module/thread"
ns_param   maxhandles      0 ;# per-thread handle
ns_param   datasource      "user='$user' password='$password' dbname='$dbname' connect_timeout=5"

ns_section "ns/server/server1/module/thread2"
ns_param   maxhandles      0 ;# per-thread handle
ns_param   datasource      "invalid data source"
