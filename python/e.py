# This python file use the following encoding: utf-8

from eapi import (
    theme_object_set
)

""" theme_object_set(obj, gadget_name, group_name)

You must use this function to load additional groups from your
edje file. This function will search the group first in the E theme
and then in your local edje file.

Args:
    obj: the object where the group will be loaded (Edje or ElmLayout)
    gadget_name: the name of your gadget
    group_name: the name of the edje group to load

"""

class Gadget(object):
    """ The gadgets base class.

    This is the base class for all gadgets, all of them MUST inherit from
    this base. Note that your class need to be called 'Gadget'.

    """

    def __init__(self):
        """ Class initialiser.

        This is called as soon as one instance of the gadget is required.
        Here you can initialize the stuff needed for your gadget to work.

        Don't forget to call the super of the base using:
          super().__init__()

        """
        self._instances = list()
        self._popups = list()

    def instance_created(self, obj, site):
        """ A new instance of the gadget has been created.

        When a gadget is placed in a container (a shelf, the desktop, etc..)
        edgar automatically create a new edje object using the group
        e/gadgets/name/main provided in the gadget edje file.
        This function is called just after the creation of the object so
        that you can fill it as required.

        The default implementation put the object in the _instances list, so
        don't forget to call the super() function.

        Args:
            obj: the created edje object
            site: the place where the gadget has been created, can be one
                of the E_GADCON_SITE_* definition.

        """
        self._instances.append(obj)

    def instance_destroyed(self, obj):
        """ An instance of the gadget has been removed.

        This is called when a gadget is removed from a site.
        You can use this function to cleanup your stuff relative to this
        instance.
        The edje object will be automatically deleted by edgar just after
        this call, you dont need to delete it yourself.

        The default implementation remove the object in the _instances list, so
        don't forget to call the super() function.

        Args:
            obj: the edje object that will be deleted

        """
        self._instances.remove(obj)

    def instance_orient(self, obj, generic, specific):
        """ The gadget need to be oriented.

        This function is needed to correctly orient the gadget, it will be
        called once on gadget initialization and also when the user change the
        orientation.

        Args:
            obj: the edje object of the gadget
            generic: The new orientation in a generic fashion, can be one of:
                E_GADCON_ORIENT_[ FLOAT / HORIZ / VERT ]
            specific: The new orientaion, can be one of:
                E_GADCON_ORIENT_*

        """
        pass

    def popup_created(self, obj):
        """ A new popup for the gadget has been created.

        Edgar automatically create the popup for the gadget, using the group
        provided in the edje file (e/gadgets/name/popup).
        Once the popup has been created this function is called so that you
        can fill it's content as required.

        The default implementation put the object in the _popups list, so
        don't forget to call the super() function.

        Args:
            obj: the created edje object that you can fill
        
        """
        self._popups.append(obj)

    def popup_destroyed(self, obj):
        """ The popup has been dismissed.

        When a popup id destroyed this function is called so that you can
        clean your stuff related to this popup.
        The edje object will be automatically deleted by edgar just after
        this call, you dont need to delete it yourself.

        The default implementation remove the object in the _popups list, so
        don't forget to call the super() function.

        Args:
            obj: the popup edje object

        """
        self._popups.remove(obj)


""" E_Gadcon_Site """
E_GADCON_SITE_UNKNOWN = 0
E_GADCON_SITE_SHELF = 1
E_GADCON_SITE_DESKTOP = 2
E_GADCON_SITE_TOOLBAR = 3
E_GADCON_SITE_EFM_TOOLBAR = 4

""" E_Gadcon_Orient """
E_GADCON_ORIENT_FLOAT = 0
E_GADCON_ORIENT_HORIZ = 1
E_GADCON_ORIENT_VERT = 2
E_GADCON_ORIENT_LEFT = 3
E_GADCON_ORIENT_RIGHT = 4
E_GADCON_ORIENT_TOP = 5
E_GADCON_ORIENT_BOTTOM = 6
E_GADCON_ORIENT_CORNER_TL = 7
E_GADCON_ORIENT_CORNER_TR = 8
E_GADCON_ORIENT_CORNER_BL = 9
E_GADCON_ORIENT_CORNER_BR = 10
E_GADCON_ORIENT_CORNER_LT = 11
E_GADCON_ORIENT_CORNER_RT = 12
E_GADCON_ORIENT_CORNER_LB = 13
E_GADCON_ORIENT_CORNER_RB = 14
