
# Misc tests

# #1: "speedy /dev/null" wouldn't work
# #2: if backendprog is not executable, should get an error message, not
# a coredump

print "1..2\n";

my $out = `$ENV{SPEEDY} /dev/null`;
my $ok = $out eq '' && $? == 0;
print $ok ? "ok\n" : "failed\n";

utime time, time, 't/scripts/basic.1';
sleep 1;

my $save = $ENV{SPEEDY_BACKENDPROG};
$ENV{SPEEDY_BACKENDPROG} = '/non-existent';
$out = `$ENV{SPEEDY} t/scripts/basic.1 2>&1`;
$ok = ($? & 255) == 0 && $out =~ /cannot spawn/i;
#print STDERR "out=$out status=$?\n";
print $ok ? "ok\n" : "failed\n";
$ENV{SPEEDY_BACKENDPROG} = $save;
