--perl
use strict;

use File::Basename;
use IO::File;
use lib "lib/";
use My::Find;
use IO::Select;
use IO::Socket::INET;

#
# Look for rdrs library, if not found: skip test.
#

#
# Set up paths
#
my $vardir = $ENV{MYSQLTEST_VARDIR} or die "Need MYSQLTEST_VARDIR";
my $mysql_test_dir = $ENV{MYSQL_TEST_DIR} or die "Need MYSQL_TEST_DIR";
my $basedir = dirname($mysql_test_dir);
my $version = $ENV{MYSQL_SERVER_VERSION};

# In the RPM case, binaries and libraries are installed in the
# default system locations, instead of having our own private base
# directory. And we install "usr/share/mysql-test". Moving up two
# more directory relative to "mysql-test" gives us a usable base
# directory for RPM installs.
if (! -d "$basedir/sql" and ! -d "$basedir/bin" )
{
  $basedir= dirname($basedir);
}

my $bindir = $ENV{MTR_BINDIR} || $basedir; # Out of source set MTR_BINDIR

#
# Check if the needed libs are available
#

# The expected installation locations are lib, lib/mysql, lib64/mysql, lib/x86_64-linux-gnu

my $F = IO::File->new("$vardir/tmp/have_rdrs_result.inc", 'w') or die;
print $F "# Looking for rdrs version $version\n";


#---------------------------- ndbclient -----------------------------------------

my $ndbclient_lib= my_find_file($bindir,
                                [ "library_output_directory",
                                  "lib*/*",
                                  "lib"
                                ],
                                [ "ndbclient.dll",
                                  "libndbclient.so",
                                  "libndbclient.dylib"
                                ],
                                NOT_REQUIRED);

if ($ndbclient_lib) {
  my $ndbclient_lib_dir = dirname($ndbclient_lib);
  print $F "--let \$RDRS_NDBCLIENT_LIB= $ndbclient_lib_dir\n"; 
  print $F "--echo Found RonDB client library\n"
} else {
  # Test runs only when ndb is compiled and enabled.
  # If not able to find libndbclient, probably an issue with this script.
  print $F "die Did not find ndbclient library\n";
}

#---------------------------- rdrclient -----------------------------------------

my $rdrclient_lib = my_find_file($bindir,
                                [ "library_output_directory",
                                  "lib*/*",
                                  "lib"
                                ],
                                [ "rdrclient.dll",
                                  "librdrclient.so",
                                  "librdrclient.dylib"
                                ],
                                NOT_REQUIRED);

if ($rdrclient_lib) {
   my $rdrclient_lib_dir = dirname($rdrclient_lib);
   print $F "--let \$RDRS_CLIENT_LIB= $rdrclient_lib_dir\n";
   print $F "--echo Found RonDB REST server client library\n"
} else {
  print $F "--let \$skip_test=1\n";
  print $F "--let \$skip_reason=This test requires RonDB REST library\n";
}

#----------------------------- rdrs_string --------------------------------------

my $rdrs_string_lib = my_find_file($bindir,
                                [ "library_output_directory",
                                  "lib*/*",
                                  "lib"
                                ],
                                [ "rdrs_string.lib",
                                  "librdrs_string.a",
                                  "librdrs_string.a"
                                ],
                                NOT_REQUIRED);

if ($rdrs_string_lib) {
  my $rdrs_string_lib_dir = dirname($ndbclient_lib);
  print $F "--let \$NDBCLIENT_LIB_DIR= $rdrs_string_lib_dir\n";
  print $F "--echo Found ndbclient library\n"
} else {
  print $F "--let \$skip_test=1\n";
  print $F "--let \$skip_reason=This test requires rdrs_string library\n";
}

#----------------------------- paths and valiables ------------------------------

print $F "--let RDRS_CONFIG_FILE=$vardir/tmp/rdrs_config.json\n";
print $F "--let RDRS_LIB_DIR=$bindir/library_output_directory\n";
print $F "--let LD_LIBRARY_PATH=$bindir/library_output_directory\n";
print $F "--let CGO_LDFLAGS=\"-g -O2 -L$bindir/lib -Wl,-rpath,$bindir/lib -lrdrclient\"\n";
print $F "--let RDRS_SRC_DIR=$ENV{'MYSQL_BASEDIR'}/storage/ndb/rest-server/rest-api-server\n";

$F->close();

#----------------------------- available ports-----------------------------------

# Get available ports for REST server                          
my $rest_server = new IO::Socket::INET(Listen    => 1, 
                                    LocalAddr => 'localhost',
                                    Proto     => 'tcp',);
my $rest_server_port = $rest_server->sockport();

my $grpc_server = new IO::Socket::INET(Listen    => 1, 
                                    LocalAddr => 'localhost',
                                    Proto     => 'tcp',);
my $grpc_server_port = $grpc_server->sockport();
 
$rest_server = undef;
$grpc_server = undef;


my $CONF = IO::File->new("$vardir/tmp/rdrs_config.json", 'w') or die;
my $jsonStr = <<"END_TXT";
{
        "REST": {
                "ServerPort": $rest_server_port
        },
        "GRPC": {
                "ServerPort": $grpc_server_port
        },
        "RonDB": {
                "Mgmds": [
                        {
                                "IP": "localhost",
                                "Port": $ENV{'MASTER_NDB_MGMD_PORT'}
                        }
                ]
        },
        "RonDBMetadataCluster": {
                "Mgmds": [
                        {
                                "IP": "localhost",
                                "Port": $ENV{'SLAVE_NDB_MGMD_PORT'}
                        }
                ]
        },
        "Security": {
                "TLS": {
                        "EnableTLS": true,
                        "RequireAndVerifyClientCert": true
                }, 
                "APIKey": {
                        "UseHopsworksAPIKeys": true
                }
        },
        "Log": {
                "Level": "info"
        },
        "Testing": {
                "MySQL": {
                        "Servers": [
                                {
                                        "IP": "localhost",
                                        "Port": $ENV{MASTER_MYPORT}
                                }
                        ],
                        "User": "root",
                        "Password": ""
                },
                "MySQLMetadataCluster": {
                        "Servers": [
                                {
                                        "IP": "localhost",
                                        "Port": $ENV{SLAVE_MYPORT}
                                }
                        ],
                        "User": "root",
                        "Password": ""
                }
        }
}
END_TXT

printf $CONF $jsonStr;

$CONF->close();

EOF

#--------------------------------------------------------------------------------

--source $MYSQLTEST_VARDIR/tmp/have_rdrs_result.inc

# skip if the required libs were not found
if ($skip_test) {
  --remove_file $MYSQLTEST_VARDIR/tmp/rdrs_config.json
  --remove_file $MYSQLTEST_VARDIR/tmp/have_rdrs_result.inc
  --skip $skip_reason
}

