
my $sp = './speedy';
my $tmpbase = "/tmp/speedy_test.shutdown";
my $tmpnum;
my $tmp;
my @tmp;
sub mkscript {
    my $tmp = $tmpbase . ++$tmpnum;
    unlink($tmp);
    open(F, ">$tmp") || die("${tmp}: $!");
    print F @_;
    close(F);
    push(@tmp, $tmp);
    return $tmp;
}

print "1..1\n";

# Test 1
my $testf = "${tmpbase}.shutdown_done.$$";
unlink($testf);

# Make a script that will create a file when it is shut down.
$tmp = mkscript <<END;
use CGI::SpeedyCGI;
CGI::SpeedyCGI->new->set_shutdown_handler(sub {open(F, ">$testf")});
END

# Run twice by setting the maximum number of runs.
# After the first run, the file should not exist,
# but after the second run, it should exist.
`$sp -- -r2 $tmp`;
sleep 1;
if (-f $testf) {
    print "failed\n";
} else {
    `$sp $tmp`;
    sleep 1;
    print -f $testf ? "ok\n" : "failed\n";
}
