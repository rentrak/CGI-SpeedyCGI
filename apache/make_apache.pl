#
# Type "perl make_apache.pl" to configure apache with mod_speedcgi support.
#

use strict;
use File::Copy;

my $libsp = 'libspeedy/blib/arch/auto/libspeedy/libspeedy.a';

my $spdir = `pwd`; chomp $spdir;
$spdir =~ s/\/apache$//;

print "\nSpeedyCGI source directory [$spdir]: ";
my $x = <STDIN>; chomp $x;
$spdir = $x || $spdir;

# Must have libspeedy already compiled.
if (! -f "$spdir/$libsp") {
    print STDERR "Cannot locate $spdir/$libsp -- please compile SpeedyCGI\n";
    exit 1;
}

print "\nApache source directory: ";
my $apdir = <STDIN>; chomp $apdir;
if (! -d "$apdir/src/modules") {
    print STDERR "Cannot locate $apdir/src/modules -- source not installed?\n";
    exit 1;
}

print "\nApache installation directory [/usr/local/apache]: ";
my $apinst = <STDIN>; chomp $apinst;
$apinst = $apinst || '/usr/local/apache';

my $md = "$apdir/src/modules/speedycgi";

print "\nBuilding $md ...\n";
mkdir $md, 0777;
chdir $md || die "${md}: $1\n";

print "Making Makefile.tmpl\n";
open(F, ">Makefile.tmpl");
print F <<END;

EXTRA_INCLUDES=-I$spdir/libspeedy

#Dependencies

\$(OBJS) \$(OBJS_PIC): Makefile

# DO NOT REMOVE
END
close(F);

print "Copying mod_speedycgi.c\n";
copy("$spdir/apache/mod_speedycgi.c", "mod_speedycgi.c");
unlink("mod_speedycgi.o");

print "Extracting libspeedy.a objects\n";
system("ar x $spdir/$libsp");
my $objs = `ls *.o`; $objs =~ s/\n/ /g;

my $confcmd = "./configure --prefix=$apinst --activate-module=src/modules/speedycgi/libspeedycgi.a --disable-shared=speedycgi";

print "\nReady to configure Apache.\nConfigure command [$confcmd]: ";
my $x = <STDIN>; chomp $x;
$confcmd = $x || $confcmd;

chdir $apdir;
system("$confcmd");

print "\nConfigure command completed.\n";
print "Fixing speedycgi Makefile\n";
chdir $md;
rename('Makefile', 'Makefile.old');
open(I, 'Makefile.old');
open(O, '>Makefile');
while (<I>) {
    s/^OBJS=.*/OBJS=mod_speedycgi.o $objs/;
    print O $_;
}
close(I);
close(O);

print "\nReady to run \"make\" to build Apache.\n";
print "Press enter to continue or control-C to abort:";
<STDIN>;
chdir $apdir;
system("make");

print "\nReady to run \"make install\" to install Apache.\n";
print "Press enter to continue or control-C to abort:";
<STDIN>;
system("make install");

print "\nDone with mod_speedycgi install!\n";
print "Please see the README in this directory for configuration instructions\n";
