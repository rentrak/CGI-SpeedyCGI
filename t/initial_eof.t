
# Solaris's select() won't wake up when stdin is initially EOF.
# To reproduce this bug, compile on Solaris with USE_SELECT defined.

print "1..1\n";

my $sp = './speedy';

sub wakeup { print "failed\n"; exit }

$SIG{ALRM} = \&wakeup;
alarm(2);

my $line = `$sp t/scripts/initial_eof </dev/null`;
print "ok\n";
