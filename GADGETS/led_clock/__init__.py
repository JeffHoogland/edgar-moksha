# This python file use the following encoding: utf-8

import sys
import os
import datetime
import random
import e

from efl import evas
from efl import edje
from efl import ecore

__gadget_name__ = 'Led Clock'
__gadget_vers__ = '0.1'
__gadget_auth__ = 'DaveMDS'
__gadget_mail__ = 'dave@gurumeditation.it'
__gadget_desc__ = 'The usual led clock.'
__gadget_vapi__ = 1


COLORS = ('off', 'red', 'green', 'blu')

class Gadget(e.Gadget):

    def instance_created(self, obj, site):
        super().instance_created(obj, site)

        timer = ecore.Timer(1.0, self.clock_timer_cb, obj)
        obj.data['timer'] = timer

        obj.signal_callback_add('mouse,down,1', '*', self.led_click_cb)

        w, h = obj.size_min
        obj.size_hint_aspect = evas.EVAS_ASPECT_CONTROL_BOTH , w, h
        return obj

    def instance_destroyed(self, obj):
        super().instance_destroyed(obj)

        timer = obj.data.get('timer')
        if timer: timer.delete()
        timer2 = obj.data.get('timer2')
        if timer2: timer2.delete()

    def clock_timer_cb(self, obj):
        time = datetime.datetime.now()
        self.draw_digit(obj, int(time.hour / 10), 1)
        self.draw_digit(obj, int(time.hour % 10), 2);
        self.draw_digit(obj, int(time.minute / 10), 3);
        self.draw_digit(obj, int(time.minute % 10), 4);
        return ecore.ECORE_CALLBACK_RENEW

    def draw_digit(self, obj, digit, position):
        # build the "framebuffer"
        if position == 1:
            fb = [0, 0, 0]
        elif position == 3:
            fb = [0, 0, 0, 0, 0, 0]
        else:
            fb = [0, 0, 0, 0, 0, 0, 0, 0, 0]

        # turn on the first N 'led' on the array (N = digit)
        for i in range(digit):
            fb[i] = 1;

        # scramble the grid
        random.shuffle(fb)

        # render to the 'fb'
        if position == 1:
            for i in range(len(fb)):
                self.led_set(obj, i + 1, 'red' if fb[i] else 'off')
        elif position == 2:
            for i in range(len(fb)):
                self.led_set(obj, i + 4, 'green' if fb[i] else 'off')
        elif position == 3:
            for i in range(len(fb)):
                self.led_set(obj, i + 13, 'blu' if fb[i] else 'off')
        elif position == 4:
            for i in range(len(fb)):
                self.led_set(obj, i + 19, 'red' if fb[i] else 'off')


    def led_set(self, obj, led, color):
        obj.signal_emit('led,set,' + color, 'led%d' % led)

    def clear_all(self, obj):
        for i in range(1, 28):
            self.led_set(obj, i, 'off')


    ## Animations
    def led_click_cb(self, obj, signal, source):
        if   source == 'led1': start_func = self.start_anim_1
        elif source == 'led2': start_func = self.start_anim_2
        elif source == 'led3': start_func = self.start_anim_3
        else: return

        timer = obj.data.get('timer')
        if timer: timer.delete()
        timer2 = obj.data.get('timer2')
        if timer2: timer2.delete()
        obj.data['timer2'] = ecore.Timer(10.0, self.stop_anim, obj)
        start_func(obj)

    def stop_anim(self, obj):
        self.clear_all(obj)
        del obj.data['timer2']
        obj.data['timer'].delete()
        obj.data['timer'] = ecore.Timer(1.0, self.clock_timer_cb, obj)

        return ecore.ECORE_CALLBACK_CANCEL


    ## ANIM 1 (full colors)
    def start_anim_1(self, obj):
        obj.data['timer'] = ecore.Timer(0.4, self.animator_1, obj)
        obj.data['color'] = 0

    def animator_1(self, obj):
        for i in range(1, 28):
            self.led_set(obj, i, COLORS[obj.data['color'] % 4])
        obj.data['color'] += 1
        return ecore.ECORE_CALLBACK_RENEW


    ## ANIM 2 (colors carousell)
    def start_anim_2(self, obj):
        self.clear_all(obj)
        obj.data['timer'] = ecore.Timer(0.02, self.animator_2, obj)
        obj.data['color'] = 1
        obj.data['count'] = 27

    def animator_2(self, obj):
        self.led_set(obj, obj.data['count'], COLORS[obj.data['color'] % 4])
        obj.data['count'] -= 1
        if obj.data['count'] <= 0:
            obj.data['count'] = 27
            obj.data['color'] += 1
        return ecore.ECORE_CALLBACK_RENEW

    ## ANIM 3 (randomize)
    def start_anim_3(self, obj):
        obj.data['timer'] = ecore.Timer(0.075, self.animator_3, obj)

    def animator_3(self, obj):
        for i in range(1, 28):
            self.led_set(obj, i, random.choice(COLORS))
        return ecore.ECORE_CALLBACK_RENEW
