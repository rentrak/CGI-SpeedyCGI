
# The backend tries to keep some file descriptors open between perl runs.
# But if the perl program messes up those fds it should be able to recover.
# Still can't recover current directory.

my $scr = 't/scripts/mungefds';

print "1..1\n";

utime time, time, $scr;
sleep 1;

sub doit {
    open(F, "$ENV{SPEEDY} $scr |");
    my @lines = <F>;
    close(F);
    return @lines;
}

@first = &doit;
sleep 1;
@second = &doit;

## print "first=",join(":", @first), "\n";
## print "second=",join(":", @second), "\n";

if (@first != 1 || $first[0] ne $second[0]) {
    print "fail\n";
} else {
    print "ok\n";
}
