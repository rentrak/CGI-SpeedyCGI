
# Solaris's select() won't wake up when stdin is initially EOF.
# To reproduce this bug, compile on Solaris with USE_SELECT defined.

# SGI's select won't wake up when script output is redirected to /dev/null.

print "1..2\n";

sub wakeup { print "failed\n"; exit }

$SIG{ALRM} = \&wakeup;

alarm(3);
my $line = `$ENV{SPEEDY} t/scripts/initial_eof </dev/null`;
print $line =~ /ok/ ? "ok\n" : "failed\n";

alarm(3);
$line = `$ENV{SPEEDY} t/scripts/initial_eof <t/scripts/initial_eof >/dev/null`;
print $? ? "failed\n" : "ok\n";

alarm(0);
