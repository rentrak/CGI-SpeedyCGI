
my $sp = './speedy';

print "1..1\n";

# Test 2
my $scr = 't/scripts/basic.2';
my $num;
utime time, time, $scr;
print STDERR "(this will take several minutes)...";
for (my $i = 1; $i <= 9000; ++$i) {
    $num = `$sp $scr`;
    print STDERR "num=$num i=$i\n";
    if ($num != $i) {
	print "failed\n";
	exit;
    }
}
print "ok\n";
