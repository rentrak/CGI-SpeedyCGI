
# In rev 1.7.2 speedy forgot to check for EAGAIN after doing a write
# on a non-blocking file descriptor.

# Also run the test for stdin, since read is in different code.  stderr
# is done with the same code as stdout, so no need for a test.


my $sp		= './speedy';
my $num		= 10000;
my $tmp         = "/tmp/speedy_eagain.$$";
my $sleeplen	= 5;

$SIG{PIPE} = IGNORE;

print "1..2\n";

&do_test(0);
&do_test(1);

sub do_test { my($do_stdin) = @_;

    if ($do_stdin) {
	open(F, "| $sp t/scripts/stdio 0 >$tmp");
    } else {
	open(F, ">$tmp");
    }
    for (my $i = 0; $i < $num; ++$i) {
	print F "$i\n";
	# Sleep in the middle of sending data.  Should cause the error.
	sleep($sleeplen) if ($do_stdin && $i == $num/2);
    }
    close(F);

    my $ok = 1;
    if ($do_stdin) {
	open(F, "<$tmp");
    } else {
	open(F, "$sp t/scripts/stdio 0 <$tmp |");
    }
    for (my $i = 0; $i < $num; ++$i) {
	$_ = <F>;
	# print STDERR "got $_\n";

	# Sleep halfway through, and if bug is present we should get an error
	# because the buffer is flushed instead of being retried.
	sleep($sleeplen) if (!$do_stdin && $_ == $num/2);

	if ($_ != $i) {
	    $ok = 0; last;
	}
    }
    close(F);
    print $ok ? "ok\n" : "failed\n";

    unlink($tmp);
}
