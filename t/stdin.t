print "1..1\n";

my $sp = './speedy t/scripts/stdin';

# Stick an extra line into stdin.
open(S, "| $sp >/dev/null");
print S "line1\nline2\n";
close(S);

# See if we get the correct output next time, or if stdin was buffered.
open(S, "| $sp >/tmp/stdin.$$");
print S "line3\n";
close(S);

open(F, "</tmp/stdin.$$");
my $line = <F>;
print ($line eq "line3\n" ? "ok\n" : "fail\n");
close(F);
