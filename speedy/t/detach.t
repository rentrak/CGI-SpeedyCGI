#
# If the frontend's BufSizGet is large enough, then the frontend should
# detach from the backend and allow it to handle another request, regardless
# of how long the frontend takes to consume the output
#

use strict;
use IO::File;

my $smbuf	=   8 * 1024;
my $lgbuf	= 512 * 1024;
my $scr		= 't/scripts/detach';

use vars qw(@open_files @pids);

sub doit { my $sz = shift;
    my($fh, $pid);
    sleep 1;

    # Keep file open
    push(@open_files, $fh = IO::File->new);

    $| = 1; print ""; $| = 0;
    if (open($fh, "-|") == 0) {
	open(F, "$ENV{SPEEDY} -- -B$sz $scr |");
	print scalar <F>;
	close(STDOUT);
	sleep 5;	# Simulate slow drain of output
	exit;
    }
    chop($pid = <$fh>);
    return $pid;
}

sub result { my $ok = shift;
    print $ok ? "ok\n" : "failed\n";
}

print "1..2\n";

utime time, time, $scr;

# With a large enough buffer, backend should detach and we get same pids
@pids = (&doit($lgbuf), &doit($lgbuf));
## print STDERR join(' ', 'pids=', @pids, "\n");
&result($pids[0] && $pids[0] == $pids[1]);

# Test the test - a small buffer should give different pids
@pids = (&doit($smbuf), &doit($smbuf));
## print STDERR join(' ', 'pids=', @pids, "\n");
&result($pids[0] && $pids[1] && $pids[0] != $pids[1]);
