# This python file use the following encoding: utf-8

from operator import itemgetter
import psutil

import e

from efl import ecore
from efl import evas
from efl import edje
from efl.evas import EXPAND_BOTH, FILL_BOTH
from efl.elementary import Box, Label, Entry, Icon, Genlist, GenlistItemClass, \
    ELM_OBJECT_SELECT_MODE_NONE, ELM_LIST_COMPRESS


__gadget_name__ = 'CPU Monitor'
__gadget_vers__ = '0.1'
__gadget_auth__ = 'DaveMDS'
__gadget_mail__ = 'dave@gurumeditation.it'
__gadget_desc__ = 'Multicore CPU monitor'
__gadget_vapi__ = 1
__gadget_opts__ = { 'popup_on_desktop': False }


class Gadget(e.Gadget):

    def __init__(self):
        super().__init__()

        self.num_cores = len(psutil.cpu_percent(interval=0, percpu=True))
        self.aspect = None
        self.main_poller = None
        self.popups_poller = None
        self.popups_itc = GenlistItemClass(item_style='default',
                                           text_get_func=self.gl_text_get,
                                           content_get_func=self.gl_content_get)

    def instance_created(self, obj, site):
        super().instance_created(obj, site)

        if self.aspect is None:
            w, h = obj.data_get('aspect_w'), obj.data_get('aspect_h')
            self.aspect = int(w), int(h)

        obj.data['bars'] = list()
        for i in range(self.num_cores):
            bar = edje.Edje(obj.evas, size_hint_expand=EXPAND_BOTH,
                            size_hint_fill=FILL_BOTH)
            e.theme_object_set(bar, 'cpu', 'bar')
            obj.part_box_append('main.box', bar)
            bar.show()
            obj.data['bars'].append(bar)

        obj.size_hint_aspect = (evas.EVAS_ASPECT_CONTROL_BOTH,
                                self.aspect[0] * self.num_cores, self.aspect[1])

        if self.main_poller is None:
            self.main_poller = ecore.Poller(8, self.main_poller_cb)

    def instance_destroyed(self, obj):
        super().instance_destroyed(obj)

        if len(self._instances) < 1 and self.main_poller is not None:
            self.main_poller.delete()
            self.main_poller = None

    def main_poller_cb(self):
        percents = psutil.cpu_percent(interval=0, percpu=True)

        for obj in self._instances:
            for i, bar in enumerate(obj.data['bars']):
                bar.message_send(0, percents[i])

        return ecore.ECORE_CALLBACK_RENEW

    def popup_created(self, popup):
        super().popup_created(popup)

        box = Box(popup)
        popup.part_swallow('main.swallow', box)
        box.show()

        en = Entry(popup, single_line=True, editable=False)
        en.text_style_user_push("DEFAULT='font_weight=Bold'")
        box.pack_end(en)
        en.show()
        popup.data['head'] = en

        li = Genlist(popup, homogeneous=True, mode=ELM_LIST_COMPRESS,
                     select_mode=ELM_OBJECT_SELECT_MODE_NONE,
                     size_hint_expand=EXPAND_BOTH, size_hint_fill=FILL_BOTH)
        box.pack_end(li)
        li.show()
        popup.data['list'] = li

        self.popups_poller_cb()
        if self.popups_poller is None:
            self.popups_poller = ecore.Poller(16, self.popups_poller_cb)

    def popup_destroyed(self, popup):
        super().popup_destroyed(popup)

        if len(self._popups) < 1 and self.popups_poller is not None:
            self.popups_poller.delete()
            self.popups_poller = None
        
    def popups_poller_cb(self):
        # build an orderd list of all running procs (pid, name, cpu_perc, mun_t)
        if psutil.version_info[0] < 2:
            self.top_procs = [ (p.pid, p.name,
                                p.get_cpu_percent(interval=0) / self.num_cores,
                                p.get_num_threads())
                               for p in psutil.process_iter() ]
        else:
            self.top_procs = [ (p.pid, p.name(),
                                p.cpu_percent(interval=0) / self.num_cores,
                                p.num_threads())
                               for p in psutil.process_iter() ]
        self.top_procs.sort(key=itemgetter(2), reverse=True)

        # update all the visible genlists
        for popup in self._popups:
            li = popup.data['list']

            # adjust the size (items count) of the genlist
            items_count = li.items_count()
            procs_count = len(self.top_procs)
            if procs_count > items_count:
                for idx in range(items_count, procs_count):
                    li.item_append(self.popups_itc, idx)
            elif procs_count < items_count:
                for idx in range(procs_count, items_count):
                    li.last_item.delete()

            # update visible list items and the header text
            li.realized_items_update()
            popup.data['head'].text = '{} Running processes'.format(procs_count)

        return ecore.ECORE_CALLBACK_RENEW

    def gl_text_get(self, gl, part, idx):
        pid, name, cpu, num_t = self.top_procs[idx]
        if num_t > 1:
            return '[{}] {} ({} threads)'.format(pid, name, num_t)
        else:
            return '[{}] {}'.format(pid, name)

    def gl_content_get(self, gl, part, idx):
        pid, name, cpu, num_t = self.top_procs[idx]
        if part == 'elm.swallow.end':
            return Label(gl, text='{0:.1f} %'.format(cpu))
        if part == 'elm.swallow.icon':
            try:
                return Icon(gl, standard=name)
            except RuntimeWarning: # icon not found
                return None
