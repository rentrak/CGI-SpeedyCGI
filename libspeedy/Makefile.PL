
use strict;
use ExtUtils::MakeMaker;

WriteMakefile(
    NAME		=> 'libspeedy',
    OBJECT		=> 'speedy_frontend.o speedy_queue.o speedy_libutil.o',
    LINKTYPE		=> 'static',
    INC			=> '-I.. -I.',
    DEFINE              => '-DSPEEDY_BIN=\"$(INSTALLBIN)/speedy\"',
);
