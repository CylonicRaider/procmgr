=====================
procmgr User's Manual
=====================

Introduction
============

This describes the use and configuration of |procmgr|_.

procmgr is a *process manager*, similar to ``init(1)``. It allows managing
processes by invoking certain user-defined actions_.

The actual process management happens by a daemon running in the background;
to invoke actions, a client (embedded in the same executable) connects to
the daemon via a UNIX domain socket and sends a request to perform a certain
action.

Command-line usage
==================


``USAGE: procmgr [-h|-V] [-c conffile] [-d [-f]|-t|-s|-r] [program action
[args ...]]``

========================= ===================================================
``-h`` (``--help``)       This help.
``-V`` (``--version``)    Print version
``-c`` (``--config``)     Configuration file location (defaults to
                          environment variable ``PROCMGR_CONFFILE``, or to
                          the compile-time constant ``DEFAULT_CONFFILE``,
                          which defaults to ``/etc/procmgr.cfg``).
``-d`` (``--daemon``)     Start daemon (as opposed to the default "client"
                          mode).
``-f`` (``--foreground``) Stay in foreground (daemon mode only).
``-t`` (``--test``)       Check whether the daemon is running.
``-s`` (``--stop``)       Signal the daemon (if any running) to stop.
``-r`` (``--reload``)     Signal the daemon (if any running) to reload its
                          configuration.
========================= ===================================================

If none of ``-dftsr`` are supplied, ``program`` and ``action`` must be
present, and contain the program and action to invoke; additional
command-line arguments may be passed to those.

Configuration
=============

To load configuration data, a derivate of the ``.ini`` format is used; all
values described are optional, although leaving some (*i.e.*, ``cmd-start``;
see Actions_) out would be unwise. The order of declarations does not matter
(apart from global values, which must be specified before any section not to
be taken as part of that section).

::

    socket-path = <communication socket path>
    allow-uid = <default numerical UID to allow>
    allow-gid = <similar to allow-uid>

    [prog-<name>]
    allow-uid = <default UID for all uid-* in this section>
    allow-gid = <default GID>
    cmd-<action> = <shell command to run for the given action>
    uid-<action> = <UID to allow to perform this action>
    gid-<action> = <GID to allow to perform this action>
    restart-delay = <seconds after which approximately to restart>

For the UID and GID fields, and ``restart-delay``, the special value ``none``
(which is equal to -1) may be used, indicating that no UID/GID should be
allowed to perform an action, or that the program should not be
automatically restarted (as it happens for every non-positive value of
restart-delay), respectively.
Arbitrarily many program sections can be specified; out of same-named
ones, only the last is considered; similarly for all values. Spacing
between sections is purely decorational, although it increases legibility.
An example::

    socket-path = /var/local/procmgr-local
    # If GID 99 is, say, wheel, and members of that group should be
    # allowed to perform actions from their own accounts.
    allow-gid = 99

    # Interaction with this would happen via "procmgr game-server ..."
    [prog-game-server]
    # If John Doe is UID 1000.
    allow-uid = 1000
    # Note the exec, to ensure procmgr sees the UID of the server itself
    # and not of the shell.
    cmd-start = exec /home/johndoe/bin/game.server
    # Note the use of the $PID variable, which (following from above)
    # is the UID of the server.
    cmd-reload = kill -HUP $PID

Actions
=======

For each action, a (shell) *script* can be defined by using the
``cmd-<action>`` configuration value; for most actions, procmgr can perform
a *default action* if no script is supplied.

Scripts are run as children of the procmgr daemon [1]_; their standard I/O
streams are connected to those of the client that caused the action.

=========== =================================================================
``start``   **Start the program.** The script's PID is noted as the PID of
            the program — therefore, the script should preferably ``exec``
            the actual program to run —, and the client exits immediately
            with a success status.

            Since procmgr cannot really know how to start some arbitrary
            program, there is no default action, and trying to start a
            program without a script for this action will fail.

``restart`` **Restart the program.** Similarly to ``start``, the program's
            PID is updated to the PID of the script; the client does not wait
            for the script to exit (again). See also the ``stop`` action.

            The default action is to ``stop`` the program and to ``start`` it
            again.

``reload``  **Reload the program's configuration.** For programs that support
            that, this can cause the program to reload its configuration
            without downtime, if properly configured. The client waits for
            the script (if any) to exit, and exits with the script's exit
            status.

            The default action is to ``restart`` the program, assuming that
            it does not support on-line reloading.

``signal``  **Arbitrary user-defined action.** This does not have any
            semantical binding; the script may do whatever it wishes.

            The default action is not to do anything and to return a success
            status.

``stop``    **Stop the program.** The script can use the ``PID`` environment
            variable to check which process to signal. Instead of waiting for
            the script to finish, procmgr will wait for the *program* to exit
            and return its exit status.

            The default action is to send the currently-running process a
            ``SIGTERM`` signal.

``status``  **Check the program's status.** The script should print a short
            message and return an exit code depending on whether the program
            is running or not.

            The default is to print ``running`` (with a terminating newline)
            and to exit with a status code of zero if there is a process
            running as the program, or ``not running`` with an exit status of
            one if there is no process running.
=========== =================================================================

Action execution
================

Actions commands are run by ``ACTION_SHELL`` (``/bin/sh``), appended after
a ``-c`` parameter; additional positional arguments are passed after
commands. The environment is empty, save for the following variables:

============ ================================================================
``PATH``     The path to get executables from. All other ones must be fetched
             by absolute path. Equal to the ``ACTION_PATH`` compile-time
             constant.
``SHELL``    The shell used to run the command. Equal to the ``ACTION_SHELL``
             compile-time constant.
``PROGNAME`` The name of the current program.
``ACTION``   The name of the action being executed now.
``PID``      The PID of the process of the current program, or the empty
             string if none.
============ ================================================================

For an action to be allowed, either the UID or the GID specified must match
the UID or GID sent by the client (the built-in client sends the EUID and the
EGID of its process), respectively, or the client must have an EUID of 0
(*i.e.*, be root).

.. [1] Each script is run in an own process group, if that matters.

.. |procmgr| replace:: ``procmgr``
.. _procmgr: https://github.com/CylonicRaider/procmgr
