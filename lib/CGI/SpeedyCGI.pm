package CGI::SpeedyCGI;

$VERSION = '1.8.3';

use strict;

sub new { my $class = shift;
    bless {}, $class;
}

use vars qw($_shutdown_handler $_i_am_speedy %_opts $_opts_changed);

# Set a handler function to be called before shutting down the Perl process
sub set_shutdown_handler { my($self, $newh) = @_;
    my $oldh = $_shutdown_handler;
    $_shutdown_handler = $newh;
    $oldh;
}

# Return true if we are running under speedycgi, false otherwise.
sub i_am_speedy { defined($_i_am_speedy); }

# Set one of the speedycgi options.
sub setopt { my($self, $opt, $val) = @_;
    $opt = uc($opt);
    my $oval = $_opts{$opt};
    $_opts{$opt} = $val;
    ++$_opts_changed if ($oval ne $val);
    $oval;
}

# Return one of the speedycgi options.
sub getopt { my($self, $opt) = @_;
    $_opts{uc($opt)};
}

1;

__END__

=head1 NAME

CGI::SpeedyCGI - Speed up CGI scripts by running them persistently

=head1 SYNOPSIS

 #!/usr/local/bin/speedy

 ### Your CGI Script Here

 ##
 ## Optionally, use the CGI::SpeedyCGI module for various things
 ##

 # Create a SpeedyCGI object
 use CGI::SpeedyCGI;
 my $sp = CGI::SpeedyCGI->new;

 # See if we are running under SpeedyCGI or not.
 print "Running under speedy=", $sp->i_am_speedy ? 'yes' : 'no', "\n";

 # Set up a shutdown handler
 $sp->set_shutdown_handler(sub { do something here });

 # Set/get some SpeedyCGI options
 $sp->setopt('timeout', 30);
 print "maxruns=", $sp->getopt('maxruns'), "\n";


=head1 DESCRIPTION

SpeedyCGI is a way to run CGI perl scripts persistently, which usually
makes them run much more quickly.  Converting scripts to use SpeedyCGI is
in most cases as simple has changing the interpreter line at the top of
the script from

    #!/usr/local/bin/perl

to

    #!/usr/local/bin/speedy

After the script is initially run, instead of exiting, SpeedyCGI keeps the
perl interpreter running in memory.  During subsequent runs, this interpreter
is used to handle new requests, instead of starting a new perl interpreter
for each execution.

SpeedyCGI conforms to the CGI specification, and does not work inside the
web server.  A very fast cgi-bin (written in C) is executed for each request.
This fast cgi-bin then contacts the persistent Perl process, which is usually
already in memory, to do the work and return the results.

Since all of these processes run outside the web server, they can't cause
problems for the web server itself.  Also, each perl program runs as its own
Unix process, so one program can't interfere with another.  Command line
options can also be used to deal with programs that have memory leaks or
other problems that might keep them from otherwise running persistently.

=head1 OPTIONS

=head2 How to Set

SpeedyCGI options can be set in several ways:

=over 4

=item Command Line

The speedy command line is the same as for regular perl, with the
exception that SpeedyCGI specific options can be passed in after a "--".

For example:

	#!/usr/local/bin/speedy -w -- -t300

at the top of your script will call SpeedyCGI with the perl option
"C<-w>" and will pass the "C<-t>" option to speedy, telling it to exit
if no new requests have been received after 300 seconds.

=back

=over 4

=item Environment

Environment variables can be used to pass in options.  This can only be
done before the initial execution (ie not from within the script itself).

=back

=over 4

=item CGI::SpeedyCGI

The CGI::SpeedyCGI module provides a method, setopt, to set options from
within the perl script at runtime.  There is also a getopt method to retrieve
the current options.

=back

=over 4

=item mod_speedycgi

If you are using the optional Apache module, SpeedyCGI options can be
set in the httpd.conf file.

=back

=head2 Options Available

The following options are available:

=over 4

=item TIMEOUT

    Command Line	: -tN
    Environment		: SPEEDY_TIMEOUT
    CGI::SpeedyCGI	: TIMEOUT
    mod_speedycgi	: SpeedyTimeout
    Default Value	: 3600 (one hour)

    Description:

	If no new requests have been received after N
	seconds, exit the persistent perl interpreter.
	Use 0 to indicate no timeout.

=item MAXRUNS

    Command Line	: -rN
    Environment		: SPEEDY_MAXRUNS
    CGI::SpeedyCGI	: MAXRUNS
    mod_speedycgi	: SpeedyMaxruns
    Default Value	: 0 (ie no max)

    Description:

	Once the perl interpreter has run N times, exit.

=item TMPBASE

    Command Line	: -Tstr
    Environment		: SPEEDY_TMPBASE
    CGI::SpeedyCGI	: n/a
    mod_speedycgi	: SpeedyTmpbase
    Default Value	: /tmp/speedy

    Description:

	Use the given prefix for creating temporary files.
	This must be a filename prefix, not a directory name.

=item BUFSIZ_POST

    Command Line	: -bN
    Environment		: SPEEDY_BUFSIZ_POST
    CGI::SpeedyCGI	: n/a
    mod_speedycgi	: n/a
    Default Value	: 1024

    Description:

	Use N bytes for the buffer that sends data
	to the CGI script.

=item BUFSIZ_GET

    Command Line	: -BN
    Environment		: SPEEDY_BUFSIZ_GET
    CGI::SpeedyCGI	: n/a
    mod_speedycgi	: n/a
    Default Value	: 8192

    Description:

	Use N bytes for the buffer that receives data
	from the CGI script.

=item MAXBACKENDS

    Command Line        : -MN
    Environment         : SPEEDY_MAXBACKENDS
    CGI::SpeedyCGI      : n/a
    mod_speedycgi       : n/a
    Default Value       : 0

    Description:

    If non-zero limits the number of backend processes that
    speedy will spawn at any given time.  This is intended
    to prevent thrashing in which a large perl library
    will cause the system to slow down to the point where
    there is an explosion of speedy processes when speedy
    spawns backends to deal with incoming requests.

=back

=head1 METHODS

The following methods are available in the CGI::SpeedyCGI module.

=over 4

=item S<new >    

Create a new CGI::SpeedyCGI object.

    my $sp = CGI::SpeedyCGI->new;

=item set_shutdown_handler($function_ref)

Register a function that will be called right before the perl interpreter
exits.  This is B<not> at the end of each request, it is when the perl
interpreter decides to exit completely (due to a timeout, maxruns, etc)

    $sp->set_shutdown_handler(sub {$dbh->logout});

=item i_am_speedy

Returns a boolean telling whether this script is running under SpeedyCGI
or not.  A CGI script can run under regular perl, or under SpeedyCGI.
This method allows the script to tell which environment it is in.

    $sp->i_am_speedy;

=item setopt($optname, $value)

Set one of the SpeedyCGI options given in the OPTIONS section.  Returns
the previous value that the option had.  $optname is case-insensitive.

    $sp->setopt('TIMEOUT', 300);

=item getopt($optname)

Return the current value of one of the SpeedyCGI options.  $optname
is case-insensitive.

    $sp->getopt('TIMEOUT');

=back

=head1 INSTALLATION

SpeedyCGI has been tried with perl versions 5.004_04 and 5.005_02,
and under Solaris 2.6, Redhat Linux 5.1, and FreeBSD 3.1.  There
may be problems wither other OSes or earlier versions of Perl.

To install, do the following:

    perl Makefile.PL
    make
    make test
    make install

This will install a "speedy" binary in the same directory where "perl"
was installed.  If you want to install the optional Apache module,
see the README in the apache directory.

=head1 BUGS

=over 4

=item *

Under heavy load we may run out of sockets (especially on FreeBSD),
since they hang around in a TIME_WAIT state after closing.  Might do
better with fifos (named pipes).

Workaround on FreeBSD is to increase the "maxusers" value in the kernel
config file and compile/install a new kernel.  The default value of 32 is
too low -- use 256 or more.

=back

=over 4

=item *

On Solaris w/Netscape Enterprise 3.x, occasionally the CGI front-end gets
stuck in the poll() call in speedy.c.

=back

=over 4

=item *

"make test" reportedly fails under sun4 sunos 4.1.4

=back

=over 4

=item *

Release 1.6 reportedly runs very slow on Dec Alpha running Unix 4.0b
and fails the intial_eof test.  1.5 runs OK.

=back


=head1 TODO

=over 4

=item *

Need benchmarks of speedy vs mod_perl

=back

=over 4

=item *

Pass file descriptors 0/1 to the Perl prog using I_SENDFD on systems
that support it (like Solaris).  Avoids the overhead of copying
through the CGI front-end.

=back

=over 4

=item *

Need to figure out whether speedyhandler still works/is useful, and if so
document how to use it.

=back

=over 4

=item *

In start_perl, use a poll() timeout instead of an alarm to implement the
timeout while waiting for an accept.  It's cleaner than a signal.

=back

=over 4

=item *

Need to allow more program control from perl via the CGI::SpeedyCGI module.
Should be able to have the perl prog wait for a new connection, etc.

=back

=over 4

=item *

Add option to check the amount of memory in use and exit when it gets
too high.

=back

=over 4

=item *

Need tests for:

=over 4

=item *

getopt, setopt and i_am_speedy methods.

=item *

multiple persistent perl processes

=item *

mod_speedycgi

=back

=back

=over 4

=item *

Add option to have a single perl process handle multiple cgi-bin's.

=back


=head1 MAILING LIST

speedycgi@newlug.org.  Subscribe by sending a message to
speedycgi-request@newlug.org with "subscribe" in the body.

Archive is at http://newlug.org/mailArchive/speedycgi


=head1 DOWNLOADING

SpeedyCGI can be retrieved from:

    http://daemoninc.com/speedycgi
    http://www.cpan.org/modules/by-authors/id/H/HO/HORROCKS/


=head1 AUTHOR

    Sam Horrocks
    Daemon Consulting Inc.
    http://daemoninc.com
    sam@daemoninc.com


=head1 COPYRIGHT

Copyright (c) 1999-2000 Daemon Consulting Inc. All rights
reserved. This program is free software; you can redistribute it
and/or modify it under the same terms as Perl itself.

