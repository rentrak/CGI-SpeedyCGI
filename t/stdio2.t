
my $sp		= './speedy';
my $do_stderr	= 1;
my $num		= 10000;
my $tmp		= "/tmp/speedy_stdio.$$";
my $max		= $do_stderr ? 4 : 2;
my @redirects	= $do_stderr ? (">$tmp", "1 2>$tmp") : (">$tmp");

print "1..$max\n";

for (my $j = 0; $j < 2; ++$j) {
    foreach my $redirect (@redirects) {
	open(F, "| $sp t/scripts/stdio2 $redirect");
	for (my $i = 0; $i < $num; ++$i) {
	    print F "$i\n";
	}
	close(F);

	my $ok = 1;
	open(F, "<$tmp");
	for (my $i = 0; $i < $num; ++$i) {
	    $_ = <F>;
	    ## print STDERR "got $_\n";
	    if ($_ != $i) {
		$ok = 0; last;
	    }
	}
	close(F);
	print $ok ? "ok\n" : "failed\n";

	unlink($tmp);
    }
}
