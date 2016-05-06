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

::

    USAGE: procmgr [-h|-V] [-c conffile] [-d [-f]|-t|-s|-r]
                   [program action [args ...]]
    -h: (--help) This help
    -V: (--version) Print version (v1.0)
    -c: (--config) Configuration file location (defaults to environment
        variable PROCMGR_CONFFILE, or to /etc/procmgr.cfg if not
        set)
    -d: (--daemon) Start daemon (as opposed to the default "client"
        mode)
    -f: (--foreground) Stay in foreground (daemon mode only)
    -t: (--test) Check whether the daemon is running
    -s: (--stop) Signal the daemon (if any running) to stop
    -r: (--reload) Signal the daemon (if any running) to reload its
        configuration
    If none of -dftsr are supplied, program and action must be present,
    and contain the program and action to invoke; additional command-line
    arguments may be passed to those.

(``etc/procmgr.cfg`` is specified by the ``DEFAULT_CONFFILE`` constant.)

Configuration
=============

To load configuration data, a derivate of the ``.ini`` format is used; all
values described are optional, although leaving some out would be unwise.
The order of declarations does not matter (apart from global values, which
must be specified before any section not to be taken as part of that
section).

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
-------

Actions commands are run by ``ACTION_SHELL`` (``/bin/sh``), appended after
a ``-c`` parameter; additional positional arguments are passed after
commands. The environment is empty, save for the following variables::

    PATH     -- The path to get executables from. All other ones must be
                fetched by absolute path. Equal to the ACTION_PATH constant.
    SHELL    -- The shell used to run the command. Equal to the ACTION_SHELL
                constant.
    PROGNAME -- The name of the current program.
    ACTION   -- The name of the action being executed now.
    PID      -- The PID of the process of the current program, or the empty
                string if none.

The PID of the process that is running the ``start`` and ``restart`` actions
is recorded as the PID of the program as a whole; thus, the command for
these actions should preferably ``exec`` the actual service to be run.

For an action to be allowed, either the UID or the GID specified must match
the UID or GID sent by the client (the built-in client sends the EUID and the
EGID of its process), respectively, or the client must have an EUID of 0
(*i.e.*, be root).

.. |procmgr| replace:: ``procmgr``
.. _procmgr: https://github.com/CylonicRaider/procmgr
