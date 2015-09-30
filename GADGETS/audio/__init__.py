# This python file use the following encoding: utf-8

import os
import sys
import dbus
from urllib.parse import unquote as url_unquote

import e

from efl import ecore
from efl import evas
from efl import edje
from efl.dbus_mainloop import DBusEcoreMainLoop
from efl.elementary.label import Label
from efl.elementary.layout import Layout
from efl.elementary.slider import Slider


__gadget_name__ = 'Audio'
__gadget_vers__ = '0.1'
__gadget_auth__ = 'DaveMDS'
__gadget_mail__ = 'dave@gurumeditation.it'
__gadget_desc__ = 'The complete audio gadget.'
__gadget_vapi__ = 1
__gadget_opts__ = { 'popup_on_desktop': True }


_instance = None


class Gadget(e.Gadget):

    def __init__(self):
        global _instance
        _instance = self

        super().__init__()

        self.player_objs = {}  # key: Player instance   val: (list of edje objs)
        self.channel_objs = {} # key: Channel instance  val: (list of elm Slider)

        self.mpris = Mpris2_Client()
        self.pulse = PulseAudio_Client()

    def instance_created(self, obj, site):
        super().instance_created(obj, site)

        obj.signal_callback_add('mouse,down,2', 'over', self.speaker_click_cb)
        obj.signal_callback_add('mouse,wheel,*', 'over', self.speaker_wheel_cb)
        obj.size_hint_aspect = evas.EVAS_ASPECT_CONTROL_BOTH , 16, 16
        self.speaker_update(obj)

    def instance_destroyed(self, obj):
        super().instance_destroyed(obj)

    def speaker_click_cb(self, obj, sig, source):
        if self.pulse.channels:
            ch = self.pulse.channels[0]
            ch.mute_toggle()

    def speaker_update(self, speaker):
        if self.pulse and len(self.pulse.channels) > 0:
            ch = self.pulse.channels[0]
            left = right = ch.volume / 655
            speaker.message_send(0, (ch.muted, left, right))

    def speaker_wheel_cb(self, obj, sig, source):
        if self.pulse.channels:
            ch = self.pulse.channels[0]
            vol = ch.volume
            if sig == 'mouse,wheel,0,1':
                new_vol = vol - 3000
            elif sig == 'mouse,wheel,0,-1':
                new_vol = vol + 3000
            else:
                return
            ch.volume_set(min(max(0, new_vol), 65500))

    def popup_created(self, popup):
        super().popup_created(popup)

        # add all the available players to the popup edje box
        for player in self.mpris.players:
            self.popup_player_add(popup, player)

        # add all the channel sliders
        if self.pulse.conn is not None:
            for ch in self.pulse.channels:
                self.popup_volume_add(popup, ch)
        else:
            lb = Label(popup, text='Cannot connect to PulseAudio')
            lb.show()
            popup.part_box_append('volumes.box', lb)

    def popup_destroyed(self, popup):
        super().popup_destroyed(popup)

        while True:
            # pop an item from the players box
            obj = popup.part_box_remove_at('players.box', 0)
            if obj is None: break

            # remove the obj from our lists
            for player, objs in self.player_objs.items():
                while obj in objs: objs.remove(obj)

            # delete the player layout
            obj.delete()

        while True:
            # pop an item from the players box
            obj = popup.part_box_remove_at('volumes.box', 0)
            if obj is None: break

            # remove the obj from our lists
            for channel, objs in self.channel_objs.items():
                while obj in objs: objs.remove(obj)

            # delete the slider
            obj.delete()

    def popup_player_add(self, popup, player):
        # create the edje obj for this player from 'e/gadgets/audio/player'
        o = Layout(popup)
        e.theme_object_set(o, 'audio', 'player')
        o.size_hint_min = o.edje.size_min

        o.signal_callback_add('act,play', '', lambda o,sig,src: player.play())
        o.signal_callback_add('act,prev', '', lambda o,sig,src: player.prev())
        o.signal_callback_add('act,next', '', lambda o,sig,src: player.next())
        o.signal_callback_add('act,rais', '', lambda o,sig,src: player.rais())

        self.player_update(o, player)
        o.show()

        # insert the player in the popup box
        popup.part_box_append('players.box', o)
        popup.size_hint_min = popup.size_min

        # keep track of this obj in the player_objs dict
        if not player in self.player_objs:
            self.player_objs[player] = []
        self.player_objs[player].append(o)

    def player_changed(self, player):
        # the mpris player has changed, update all the relative edje objects
        for o in self.player_objs.get(player, []):
            self.player_update(o, player)

    def player_added(self, player):
        for popup in self._popups:
            self.popup_player_add(popup, player)

    def player_removed(self, player):
        if player in self.player_objs:
            for o in self.player_objs[player]:
                o.delete()
            # remove the player from our list
            del self.player_objs[player]

        for popup in self._popups:
            popup.size_hint_min = popup.size_min

    def player_update(self, obj, player):
        # player name
        obj.part_text_set('player_name', player.label or player.name)

        # play/pause button
        if player.playback_status == 'Playing':
            obj.signal_emit('state,set,playing', '')
        else:
            obj.signal_emit('state,set,paused', '')

        # metadata
        txt = ''
        if 'xesam:title' in player.metadata:
            txt += '<title>%s</><br>' % player.metadata['xesam:title']
        if 'xesam:artist' in player.metadata:
            txt += '<tag>by</> %s<br>' % player.metadata['xesam:artist'][0]
        if 'xesam:album' in player.metadata:
            txt += '<tag>from</> %s<br>' % player.metadata['xesam:album']
        obj.part_text_set('metadata', txt)

        # cover image
        img = obj.content_unset('cover.swallow')
        if img: img.delete()

        if 'mpris:artUrl' in player.metadata:
            fname = url_unquote(player.metadata['mpris:artUrl'])
            fname = fname.replace('file://', '')
            try:
                img = evas.FilledImage(obj.evas, file=fname)
                obj.content_set('cover.swallow', img)
            except:
                pass

    def popup_volume_add(self, popup, channel):
        sl = Slider(popup, text=channel.name, min_max=(0, 65500),
                    size_hint_align=(evas.EVAS_HINT_FILL, 0.0))
        sl.value = channel.volume
        sl.disabled = True if channel.muted else False
        sl.callback_changed_add(self.popup_slider_changed_cb, channel)
        sl.callback_slider_drag_start_add(self.popup_slider_drag_start_cb)
        sl.callback_slider_drag_stop_add(self.popup_slider_drag_stop_cb)
        sl.event_callback_add(evas.EVAS_CALLBACK_MOUSE_DOWN,
                              self.popup_slider_click_cb, channel)
        sl.show()

        # insert the slider in the popup box
        popup.part_box_prepend('volumes.box', sl)
        popup.size_hint_min = popup.size_min

        # keep track of this obj in the channel_objs dict
        if not channel in self.channel_objs:
            self.channel_objs[channel] = []
        self.channel_objs[channel].append(sl)

    def popup_slider_changed_cb(self, slider, channel):
        channel.volume_set(slider.value)

    def popup_slider_click_cb(self, slider, event, channel):
        if event.button == 2:
            channel.mute_toggle()

    def popup_slider_drag_start_cb(self, slider):
        slider.data['dragging'] = True

    def popup_slider_drag_stop_cb(self, slider):
        del slider.data['dragging']

    def volume_changed(self, channel):
        # update all the sliders (except the one currently dragged)
        if channel in self.channel_objs:
            for sl in self.channel_objs[channel]:
                if not 'dragging' in sl.data:
                    sl.value = channel.volume
        # update all the speakers
        for speaker in self._instances:
            self.speaker_update(speaker)

    def mute_changed(self, channel):
        # update all the sliders and the speakers
        if channel in self.channel_objs:
            for sl in self.channel_objs[channel]:
                sl.disabled = True if channel.muted else False
        for speaker in self._instances:
            self.speaker_update(speaker)

    def channel_added(self, channel):
        for popup in self._popups:
            self.popup_volume_add(popup, channel)

    def channel_removed(self, channel):
        if channel in self.channel_objs:
            for sl in self.channel_objs[channel]:
                sl.delete()
            del self.channel_objs[channel][:]
            del self.channel_objs[channel]

        for popup in self._popups:
            popup.size_hint_min = popup.size_min

class Mpris2_Client(object):
    BASE_PATH = 'org.mpris.MediaPlayer2.'

    def __init__ (self):
        self.players = []
        self.bus = dbus.SessionBus(mainloop=DBusEcoreMainLoop())

        # build the list of players
        for name in self.bus.list_names():
            if name.startswith(self.BASE_PATH):
                self.player_add(name)

        # and keep the list updated when names changes
        self.bus.add_signal_receiver(self.name_owner_changed_cb,
                                     "NameOwnerChanged")

    def name_owner_changed_cb(self, name, old_owner, new_owner):
        if name.startswith(self.BASE_PATH):
            if new_owner:
                self.player_add(name)
            else:
                self.player_del(name)

    def player_add(self, obj_path):
        player = Mpris2_Player(self.bus, obj_path)
        self.players.append(player)
        _instance.player_added(player)

    def player_del(self, obj_path):
        for player in self.players:
            if player.obj_path == obj_path:
                self.players.remove(player)
                _instance.player_removed(player)
                del player
                break

class Mpris2_Player(object):
    MAIN_IFACE = 'org.mpris.MediaPlayer2'
    PLAYER_IFACE = 'org.mpris.MediaPlayer2.Player'

    def __init__(self, bus, obj_path):
        self.obj_path = obj_path
        self.label = None
        self.metadata = None # metadata dict as per mpris2 specs
        self.playback_status = 'Stopped' # or 'Playing' or 'Paused'
        self.volume = 0.0 # range: 0.0 - 1.0

        self.proxy = bus.get_object(self.obj_path, '/org/mpris/MediaPlayer2')

        # self.prop_iface.GetAll(...)
        self.prop_iface = dbus.Interface(self.proxy, dbus_interface=dbus.PROPERTIES_IFACE)
        self.prop_iface.connect_to_signal('PropertiesChanged', self.props_changed_cb)
        self.metadata = self.prop_iface.Get(self.PLAYER_IFACE, 'Metadata')
        self.playback_status = self.prop_iface.Get(self.PLAYER_IFACE, 'PlaybackStatus')
        self.volume = self.prop_iface.Get(self.PLAYER_IFACE, 'Volume')
        self.label = self.prop_iface.Get(self.MAIN_IFACE, 'Identity')

    def props_changed_cb(self, iface, props, invalidated):
        if 'Metadata' in props:
            self.metadata = props.get('Metadata')
        if 'PlaybackStatus' in props:
            self.playback_status = props.get('PlaybackStatus')
        if 'Volume' in props:
            self.volume = props.get('Volume')

        _instance.player_changed(self)

    def play(self):
        self.proxy.PlayPause(dbus_interface=self.PLAYER_IFACE)

    def next(self):
        self.proxy.Next(dbus_interface=self.PLAYER_IFACE)

    def prev(self):
        self.proxy.Previous(dbus_interface=self.PLAYER_IFACE)

    def rais(self):
        self.proxy.Raise(dbus_interface=self.MAIN_IFACE)


class AudioChannel(object):
    def __init__(self, obj, iface, name, volume, muted):
        self.obj = obj
        self.iface = iface
        self.name = name
        self.volume = int(volume[0])
        self.muted = muted

        # This do not work, only connection on the main pulse obj work...
        #   so for the moment dispatch the callback from there
        # obj.connect_to_signal('VolumeUpdated', self.volume_changed_signal_cb)

    def volume_set(self, value):
        self.volume = value
        self.obj.Set(self.iface, 'Volume', [dbus.UInt32(value)],
                     dbus_interface=dbus.PROPERTIES_IFACE)

    def mute_toggle(self):
        self.muted = not self.muted
        self.obj.Set(self.iface, 'Mute', self.muted,
                     dbus_interface=dbus.PROPERTIES_IFACE)

    def volume_changed_signal_cb(self, volume):
        self.volume = int(volume[0])
        _instance.volume_changed(self)

    def mute_changed_signal_cb(self, muted):
        self.muted = muted
        _instance.mute_changed(self)

    def __str__(self):
        return '[%s]: "%s" volume: %s' % \
               (self.iface.split('.')[-1], self.name, self.volume[:])

class PulseAudio_Client(object):
    PULSE_OBJ = '/org/pulseaudio/core1'
    PULSE_IFACE = 'org.PulseAudio.Core1'
    STREAM_IFACE = 'org.PulseAudio.Core1.Stream'
    DEVICE_IFACE = 'org.PulseAudio.Core1.Device'
    
    def __init__ (self):
        self.conn = None
        self.srv_addr = None
        self.channels = []
        self.exe_count = 0

        self.try_to_connect()

    def address_lookup(self):
        """ Search the address of the pulse dbus socket """
        # 1. try the environment var
        addr = os.environ.get('PULSE_DBUS_SERVER')
        if addr: return addr

        # 2. well-known system-wide daemon socket
        if os.access('/run/pulse/dbus-socket', os.R_OK | os.W_OK):
            return 'unix:path=/run/pulse/dbus-socket'

        # 3. dbus lookup on the SessionBus
        try:
            bus = dbus.SessionBus()
            obj = bus.get_object('org.PulseAudio1', '/org/pulseaudio/server_lookup1')
            return obj.Get('org.PulseAudio.ServerLookup1', 'Address',
                           dbus_interface=dbus.PROPERTIES_IFACE)
        except:
            return None

    def try_to_connect(self):
        print("PULSE: try_to_connect")
        if self.conn is None and self.connect() is False:
            ecore.Timer(5.0, self.try_to_connect)
        return ecore.ECORE_CALLBACK_CANCEL

    def connect(self):
        print("PULSE: connect %d", self.exe_count)
        self.srv_addr = self.address_lookup()
        try:
            self.conn = dbus.connection.Connection(self.srv_addr,
                                                   mainloop=DBusEcoreMainLoop())
        except:
            if self.exe_count < 3:
                print("PULSE: Exe")
                self.exe_count += 1
                x = ecore.Exe('pulseaudio --start')
                x.on_del_event_add(lambda *a: self.connect())
            return False
        
        self.exe_count = 0
        self.conn.call_on_disconnection(self.disconnect_cb)

        # get all available channels
        self.all_channels_add()
        # and listen for channels added/removed
        pulse = self.conn.get_object(object_path=self.PULSE_OBJ)
        for sig in ('NewSink', 'SinkRemoved',
                    'NewPlaybackStream', 'PlaybackStreamRemoved'):
            pulse.ListenForSignal('org.PulseAudio.Core1.' + sig,
                                  dbus.Array(signature='o'))
            self.conn.add_signal_receiver(self.channel_signal_cb, sig,
                                          member_keyword='signal')
        # also enable volume/mute signal from pulse
        for sig in ('Device.VolumeUpdated', 'Device.MuteUpdated',
                    'Stream.VolumeUpdated', 'Stream.MuteUpdated'):
            pulse.ListenForSignal('org.PulseAudio.Core1.' + sig,
                                  dbus.Array(signature='o'))
        # this should be connected per-object, in the Channel class...
        for sig in ('VolumeUpdated', 'MuteUpdated'):
            self.conn.add_signal_receiver(self.volume_signal_cb, sig,
                                          member_keyword='signal',
                                          path_keyword='obj_path')
        return True

    def disconnect_cb(self, conn):
        for ch in self.channels:
            _instance.channel_removed(ch)
            del ch
        self.conn = None
        self.srv_addr = None
        self.channels = []
        ecore.Timer(3.0, self.try_to_connect)

    def channel_signal_cb(self, *args, signal):
        obj_path = args[0]
        if signal == 'NewSink':
            ch = self.sink_add(obj_path)
        elif signal == 'NewPlaybackStream':
            ch = self.stream_add(obj_path)
        elif signal in ('SinkRemoved', 'PlaybackStreamRemoved'):
            for ch in self.channels:
                if ch.obj.object_path == obj_path:
                    _instance.channel_removed(ch)
                    self.channels.remove(ch)
                    del ch

    def volume_signal_cb(self, *args, signal, obj_path):
        # dispatch to the correct Channel instance
        for ch in self.channels:
            if ch.obj.object_path == obj_path:
                if signal == 'VolumeUpdated':
                    ch.volume_changed_signal_cb(*args)
                elif signal == 'MuteUpdated':
                    ch.mute_changed_signal_cb(*args)
                break

    def _fuckyoupulse(self, ay):
        return ''.join([ chr(byte) for byte in ay ])

    def all_channels_add(self):
        obj = self.conn.get_object(self.PULSE_IFACE, self.PULSE_OBJ)

        sinks = obj.Get(self.PULSE_IFACE, 'Sinks',
                        dbus_interface=dbus.PROPERTIES_IFACE)
        streams = obj.Get(self.PULSE_IFACE, 'PlaybackStreams',
                          dbus_interface=dbus.PROPERTIES_IFACE)

        # keep the default sink (if available) on top of the list
        try:
            default_sink = obj.Get(self.PULSE_IFACE, 'FallbackSink',
                                   dbus_interface=dbus.PROPERTIES_IFACE)
        except:
            pass
        else:
            if default_sink in sinks:
                sinks.remove(default_sink)
                sinks.insert(0, default_sink)

        for obj_path in sinks:
           self.sink_add(obj_path)

        for obj_path in streams:
            self.stream_add(obj_path)

    def stream_add(self, obj_path):
        try:
            obj = self.conn.get_object(self.STREAM_IFACE, obj_path)
            volume = obj.Get(self.STREAM_IFACE, 'Volume',
                             dbus_interface=dbus.PROPERTIES_IFACE)
            mute = obj.Get(self.STREAM_IFACE, 'Mute',
                           dbus_interface=dbus.PROPERTIES_IFACE)
            props  = obj.Get(self.STREAM_IFACE, 'PropertyList',
                             dbus_interface=dbus.PROPERTIES_IFACE)
        except:
            return None

        try:
            name = self._fuckyoupulse(props['application.name'])
        except:
            name = 'Unknown app'

        ch = AudioChannel(obj, self.STREAM_IFACE, name, volume, mute)
        self.channels.append(ch)
        _instance.channel_added(ch)
        return ch

    def sink_add(self, obj_path):
        try:
            obj = self.conn.get_object(self.DEVICE_IFACE, obj_path)
            volume = obj.Get(self.DEVICE_IFACE, 'Volume',
                             dbus_interface=dbus.PROPERTIES_IFACE)
            mute = obj.Get(self.DEVICE_IFACE, 'Mute',
                           dbus_interface=dbus.PROPERTIES_IFACE)
            props  = obj.Get(self.DEVICE_IFACE, 'PropertyList',
                             dbus_interface=dbus.PROPERTIES_IFACE)
        except:
            return None

        try:
            name = self._fuckyoupulse(props['device.profile.description'])
        except:
            try:
                name = self._fuckyoupulse(props['device.description'])
            except:
                name = 'Unknown device'

        ch = AudioChannel(obj, self.DEVICE_IFACE, name, volume, mute)
        self.channels.append(ch)
        _instance.channel_added(ch)
        return ch
