
# Bug: signals aren't reset between requests.

# If we send a signal to speedy, while its idle, it should exit cleanly
# and call the shutdown handler.

print "1..1\n";

my $sp = './speedy';
my $scr = 't/scripts/shutdown';
my $testf = "/tmp/speedy.shutdown_done.$$";

unlink $testf;
utime time, time, $scr;
sleep 2;
my $pid = `$sp -- -r2 $scr $testf`;
sleep 1;
kill 15, $pid;
sleep 1;
print -f $testf ? "ok\n" : "failed\n";
unlink $testf;
