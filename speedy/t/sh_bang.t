
# Check if the sh_bang line (the #! line embedded in the perl script)
# is actually being read correctly for speedy options.

# On some OSes (Linux) you'll get the #! arguments in argv when the program
# is exec'ed.  On others (Solaris) you have to read the file yourself
# to get all the options.


my $tmp = "/tmp/sh_bang.$$";

print "1..1\n";

open(F, ">$tmp") || die;
print F "#!$ENV{SPEEDY} -- -t5 -r2\nprint ++\$x;\n";
close(F);
sleep 1;
chmod 0755, $tmp;

my @nums = map {`$tmp`} (0..3);
unlink($tmp);

for (my $i = 0; $i < 4; ++$i) {
    if ($nums[$i] != ($i % 2) + 1) {
	print "failed\n";
	exit;
    }
}
print "ok\n";
