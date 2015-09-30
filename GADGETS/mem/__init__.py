# This python file use the following encoding: utf-8

import psutil

import e

from efl import ecore
from efl import evas
from efl import edje
from efl.evas import EXPAND_BOTH, FILL_BOTH
from efl.elementary import Box, Frame, Progressbar


__gadget_name__ = 'Memory Monitor'
__gadget_vers__ = '0.1'
__gadget_auth__ = 'DaveMDS'
__gadget_mail__ = 'dave@gurumeditation.it'
__gadget_desc__ = 'Ram + Swap monitor'
__gadget_vapi__ = 1
__gadget_opts__ = { 'popup_on_desktop': False }



class Gadget(e.Gadget):

    def __init__(self):
        super().__init__()

        self.poller = None
        self.mem = 0
        self.swp = 0

    def instance_created(self, obj, site):
        super().instance_created(obj, site)

        obj.size_hint_aspect = (evas.EVAS_ASPECT_CONTROL_BOTH,
                                int(obj.data_get('aspect_w')),
                                int(obj.data_get('aspect_h')))

        if self.poller is None:
            self.poller = ecore.Poller(8, self.poller_cb, ecore.ECORE_POLLER_CORE)

    def instance_destroyed(self, obj):
        super().instance_destroyed(obj)

        if len(self._instances) < 1 and self.poller is not None:
            self.poller.delete()
            self.poller = None

    def popup_created(self, popup):
        super().popup_created(popup)

        box = Box(popup)
        popup.part_swallow('main.swallow', box)
        box.show()

        # mem
        tot = self.format_mb(psutil.virtual_memory().total)
        fr = Frame(popup, text='Memory Usage (available {})'.format(tot),
                   size_hint_expand=EXPAND_BOTH, size_hint_fill=FILL_BOTH)
        box.pack_end(fr)
        fr.show()

        box2 = Box(popup, size_hint_expand=EXPAND_BOTH, size_hint_fill=FILL_BOTH)
        fr.content = box2
        box2.show()

        pb1 = Progressbar(popup, text='Total used',
                          span_size=200, size_hint_align=(1.0, 0.5))
        box2.pack_end(pb1)
        pb1.show()

        pb2 = Progressbar(popup, text='active',
                          span_size=200, size_hint_align=(1.0, 0.5))
        box2.pack_end(pb2)
        pb2.show()

        pb3 = Progressbar(popup, text='buffers',
                          span_size=200, size_hint_align=(1.0, 0.5))
        box2.pack_end(pb3)
        pb3.show()

        pb4 = Progressbar(popup, text='cached',
                          span_size=200, size_hint_align=(1.0, 0.5))
        box2.pack_end(pb4)
        pb4.show()

        # swap
        tot = self.format_mb(psutil.swap_memory().total)
        fr = Frame(popup, text='Swap Usage (available {})'.format(tot),
                   size_hint_expand=EXPAND_BOTH, size_hint_fill=FILL_BOTH)
        box.pack_end(fr)
        fr.show()

        pb5 = Progressbar(popup)
        fr.content = pb5
        pb5.show()

        # force the popup to always recalculate it's size
        popup.update_hints = True

        # force the poller to update the popup now
        popup.data['usd_pb'] = pb1
        popup.data['act_pb'] = pb2
        popup.data['buf_pb'] = pb3
        popup.data['cac_pb'] = pb4
        popup.data['swp_pb'] = pb5
        self.poller_cb(force=True)

    def poller_cb(self, force=False):
        mem = psutil.virtual_memory()
        swp = psutil.swap_memory()

        if force or mem.percent != self.mem or swp.percent != self.swp:
            for obj in self._instances:
                obj.message_send(0, (mem.percent, swp.percent))

            for popup in self._popups:
                self.update_pb(popup.data['usd_pb'], mem.used, mem.total)
                active = mem.used - mem.buffers - mem.cached
                self.update_pb(popup.data['act_pb'], active, mem.total)
                self.update_pb(popup.data['buf_pb'], mem.buffers, mem.total)
                self.update_pb(popup.data['cac_pb'], mem.cached, mem.total)
                self.update_pb(popup.data['swp_pb'], swp.used, swp.total)

            self.mem = mem.percent
            self.swp = swp.percent

        return ecore.ECORE_CALLBACK_RENEW

    def format_mb(self, val):
        return '{0:.0f} MB'.format(val / 1048576)

    def update_pb(self, pb, val, total):
        pb.value = val / total
        pb.unit_format = '{0} ({1:.0f} %%)'.format(self.format_mb(val),
                                                   val / total * 100)
        
