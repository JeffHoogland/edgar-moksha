# This python file use the following encoding: utf-8

import os
import sys
import socket

import e

from efl import ecore
from efl import evas
from efl import edje
from efl.elementary.label import Label
from efl.elementary.entry import utf8_to_markup
from efl.elementary.button import Button


__gadget_name__ = 'Dropbox'
__gadget_vers__ = '0.1'
__gadget_auth__ = 'DaveMDS'
__gadget_mail__ = 'dave@gurumeditation.it'
__gadget_desc__ = 'Dropbox info gadget.'
__gadget_vapi__ = 1
__gadget_opts__ = { 'popup_on_desktop': False }


#def DBG(msg):
#    print("DB: " + msg)
#    sys.stdout.flush()


class Gadget(e.Gadget):

    def __init__(self):
        super().__init__()
        self.db = Dropbox(self.db_status_changed_cb)

    def instance_created(self, obj, site):
        super().instance_created(obj, site)
        obj.size_hint_aspect = evas.EVAS_ASPECT_CONTROL_BOTH , 16, 16

    def instance_destroyed(self, obj):
        super().instance_destroyed(obj)

    def popup_created(self, popup):
        super().popup_created(popup)

        popup.data['lb'] = Label(popup)
        popup.part_box_append('popup.box', popup.data['lb'])
        popup.data['lb'].show()

        popup.data['bt'] = Button(popup)
        popup.data['bt'].callback_clicked_add(self.start_stop_clicked_cb)
        popup.part_box_append('popup.box', popup.data['bt'])
        popup.data['bt'].show()

        self.popup_update(popup)

    def popup_destroyed(self, popup):
        super().popup_destroyed(popup)

    def db_status_changed_cb(self):
        for icon in self._instances:
            if self.db.is_running:
                icon.signal_emit('daemon,running', '')
                icon.signal_emit('state,'+self.db.status, '')
            else:
                icon.signal_emit('daemon,not_running', '')
                icon.signal_emit('state,unwatched', '')

        for popup in self._popups:
            self.popup_update(popup)

    def popup_update(self, popup):
        if self.db.is_running:
            popup.data['lb'].text = utf8_to_markup(self.db.status_msg)
            popup.data['bt'].text = 'Stop Dropbox'
            popup.data['bt'].disabled = False
        elif self.db.is_installed:
            popup.data['lb'].text = "Dropbox isn't running!"
            popup.data['bt'].text = 'Start Dropbox'
            popup.data['bt'].disabled = False
        else:
            popup.data['lb'].text = "Dropbox isn't installed!"
            popup.data['bt'].text = 'Install Dropbox'
            popup.data['bt'].disabled = True

        # force the popup to recalculate it's size
        popup.size_hint_min = popup.size_min

    def start_stop_clicked_cb(self, btn):
        if self.db.is_running:
            self.db.stop()
        else:
            self.db.start()


class Dropbox(object):
    def __init__(self, status_changed_cb=None):
        self._status_changed_cb = status_changed_cb

        self.BASE_FOLDER = os.path.expanduser('~/Dropbox')
        self.DAEMON = os.path.expanduser('~/.dropbox-dist/dropboxd')
        self.PIDFILE = os.path.expanduser('~/.dropbox/dropbox.pid')
        self.CMD_SOCKET = os.path.expanduser('~/.dropbox/command_socket')

        self._cmd_socket = None
        self._cmd_fdh = None
        self._reply_buffer = ''
        self._status = ''
        self._status_msg = ''

        self._connect_timer()
        ecore.Timer(2.0, self._connect_timer)
        ecore.Timer(2.0, self._fetch_status_timer)

    @property
    def is_installed(self):
        return os.path.exists(self.DAEMON)

    @property
    def is_running(self):
        """ Check if the dropbox daemon is running """
        try:
            with open(self.PIDFILE, 'r') as f:
                pid = int(f.read())
            with open('/proc/%d/cmdline' % pid, 'r') as f:
                cmdline = f.read().lower()
        except:
            cmdline = ''

        return 'dropbox' in cmdline

    @property
    def is_connected(self):
        """ are we connected to the deamon socket ? """
        return self._cmd_fdh != None

    @property
    def status(self):
        """ 'up to date', 'syncing', 'unsyncable' or 'unwatched' """
        return self._status

    @property
    def status_msg(self):
        """ Long status message (more than one line) """
        return self._status_msg

    def start(self):
        """ Start the dropbox daemon """
        ecore.Exe(self.DAEMON)

    def stop(self):
        """ Stop the dropbox daemon """
        if self.is_connected:
            cmd = 'tray_action_hard_exit\ndone\n'
            try:
                self._cmd_socket.sendall(cmd.encode('utf-8'))
            except:
                self._disconnect()

    def _connect_timer(self):
        """ Try to connect to the daemon socket (if needed) """
        if self.is_connected:
            return ecore.ECORE_CALLBACK_RENEW

        if not self.is_running:
            return ecore.ECORE_CALLBACK_RENEW

        try:
            self._cmd_socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            self._cmd_socket.connect(self.CMD_SOCKET)
        except:
            self._cmd_socket = None
            return ecore.ECORE_CALLBACK_RENEW

        self._cmd_fdh = ecore.FdHandler(self._cmd_socket,
                                        ecore.ECORE_FD_READ | ecore.ECORE_FD_ERROR,
                                        self._cmd_socket_data_available)

        return ecore.ECORE_CALLBACK_RENEW

    def _disconnect(self):
        """ Disconnect from the daemon socket """
        self._cmd_fdh.delete()
        self._cmd_fdh = None
        self._cmd_socket.close()
        self._cmd_socket = None
        self._reply_buffer = ''
        self._status = ''
        self._status_msg = ''

        # alert the user that the state has changed
        self._status_changed_cb()

    def _update_status(self, new_status):
        if new_status == self._status:
            return
        self._status = new_status

    def _update_status_msg(self, new_status):
        if new_status == self._status_msg:
            return
        self._status_msg = new_status

        # alert the user that the state has changed
        self._status_changed_cb()

    def _cmd_socket_data_available(self, fdh):
        if fdh.has_error():
            self._disconnect()
            return ecore.ECORE_CALLBACK_CANCEL

        while True:
            tmp = self._cmd_socket.recv(1024)
            self._reply_buffer += tmp.decode('utf-8')
            if len(tmp) < 1024:
                break

        self._finalize_reply()
        return ecore.ECORE_CALLBACK_RENEW

    def _finalize_reply(self):
        reply = self._reply_buffer
        if reply.endswith('done\n'):
            self._reply_buffer = ''

            for cmd in reply.split('done\n'):
                if not cmd:
                    continue

                cmd = cmd.split('\n')
                if cmd[0] != 'ok':
                    return
                if len(cmd) > 1 and cmd[1].startswith('status'):
                    status = cmd[1].split('\t')
                    if len(status) == 2 and status[1] in ('up to date', 'syncing',
                                                          'unsyncable','unwatched'):
                        self._update_status(status[1])
                    elif len(status) >= 2:
                        self._update_status_msg('\n'.join(status[1:]))

    def _fetch_status_timer(self):
        if self.is_connected:
            c1 = 'icon_overlay_file_status\npath\t%s\ndone\n' % self.BASE_FOLDER
            c2 = 'get_dropbox_status\ndone\n'
            try:
                self._cmd_socket.sendall((c1 + c2).encode('utf-8'))
            except:
                self._disconnect()

        return ecore.ECORE_CALLBACK_RENEW

