# This python file use the following encoding: utf-8

import sys
import os
import e

from efl import evas

__gadget_name__ = 'Ruler (HalloWorld)'
__gadget_vers__ = '0.1'
__gadget_auth__ = 'DaveMDS'
__gadget_mail__ = 'dave@gurumeditation.it'
__gadget_desc__ = 'All gadgets sets have a ruler...'
__gadget_vapi__ = 1


class Gadget(e.Gadget):

    def __init__(self):
        super().__init__()

    def instance_orient(self, obj, generic, specific):
        super().instance_orient(obj, generic, specific)

        if generic == e.E_GADCON_ORIENT_VERT:
            obj.signal_emit('gadget,orient,vert', '')
        else:
            obj.signal_emit('gadget,orient,horiz', '')
