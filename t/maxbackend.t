my $sp = './speedy';
print "1..1\n";

open (RUN1, "$sp  -- -M2 t/scripts/maxbackend < /dev/null |");
sleep(1);
open (RUN2, "$sp  -- -M2 t/scripts/maxbackend < /dev/null |");
sleep(1);
open (RUN3, "$sp  -- -M2 t/scripts/maxbackend < /dev/null |");

$pid1 = <RUN1>; chop $pid1;
$pid2 = <RUN2>; chop $pid2;
$pid3 = <RUN3>; chop $pid3;

close(RUN1);
close(RUN2);
close(RUN3);

my $ok =  ($pid3 == $pid1 || $pid2 == $pid1); 

if ($ok) {
  kill 9, $pid1;
  kill 9, $pid2;
} else {
  kill 9, $pid3;
}

print $ok ? "ok\n" : "failed\n";
