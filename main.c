/* $Id$ */

#include "samblah.h"

int
main(int argc, char *argv[])
{
	(void)setlocale(LC_ALL, "");

	if (!smb_init())
		errx(1, "could not initialize libsmbclient, make sure you "
		    "have an (empty) $HOME/.smb/smb.conf");

	/*
	 * read environment variables, parse options and read configuration
	 * files.  if it returns, everything went fine, otherwise a message was
	 * printed and exit called.
	 */
	do_init(argc, argv);

	/*
	 * keep reading, parsing and executing commands from the
	 * command line.  on command `quit' or a ctrl+c, this function
	 * returns.
	 */
	do_interface();

	exit(0);
}
