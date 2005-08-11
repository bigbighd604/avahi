/* $Id$ */

/***
  This file is part of avahi.
 
  avahi is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation; either version 2.1 of the
  License, or (at your option) any later version.
 
  avahi is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General
  Public License for more details.
 
  You should have received a copy of the GNU Lesser General Public
  License along with avahi; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.
***/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <avahi-client/client.h>
#include <avahi-common/dbus.h>
#include <avahi-common/llist.h>
#include <avahi-common/error.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <stdlib.h>

#include "client.h"
#include "internal.h"

AvahiDomainBrowser* avahi_domain_browser_new (AvahiClient *client, AvahiIfIndex interface, AvahiProtocol protocol, char *domain, AvahiDomainBrowserType btype, AvahiDomainBrowserCallback callback, void *user_data)
{
    AvahiDomainBrowser *tmp = NULL;
    DBusMessage *message = NULL, *reply;
    DBusError error;
    char *path;

    if (client == NULL)
        return NULL;

    dbus_error_init (&error);

    message = dbus_message_new_method_call (AVAHI_DBUS_NAME, AVAHI_DBUS_PATH_SERVER,
            AVAHI_DBUS_INTERFACE_SERVER, "DomainBrowserNew");

    if (!dbus_message_append_args (message, DBUS_TYPE_INT32, &interface, DBUS_TYPE_INT32, &protocol, DBUS_TYPE_STRING, &domain, DBUS_TYPE_INT32, &btype, DBUS_TYPE_INVALID))
        goto dbus_error;

    reply = dbus_connection_send_with_reply_and_block (client->bus, message, -1, &error);

    if (dbus_error_is_set (&error) || reply == NULL)
        goto dbus_error;

    if (!dbus_message_get_args (reply, &error, DBUS_TYPE_OBJECT_PATH, &path, DBUS_TYPE_INVALID))
        goto dbus_error;

    if (dbus_error_is_set (&error) || path == NULL)
        goto dbus_error;

    tmp = malloc (sizeof (AvahiDomainBrowser));
    tmp->client = client;
    tmp->callback = callback;
    tmp->user_data = user_data;
    tmp->path = strdup (path);

    AVAHI_LLIST_PREPEND(AvahiDomainBrowser, domain_browsers, client->domain_browsers, tmp);

    return tmp;

dbus_error:
    dbus_error_free (&error);
    avahi_client_set_errno (client, AVAHI_ERR_DBUS_ERROR);

    return NULL;
}

char*
avahi_domain_browser_path (AvahiDomainBrowser *b)
{
    return b->path;
}

DBusHandlerResult
avahi_entry_group_event (AvahiClient *client, AvahiBrowserEvent event, DBusMessage *message)
{
    AvahiDomainBrowser *n, *db = NULL;
    DBusError error;
    const char *path;
    char *domain;
    int interface, protocol;

    dbus_error_init (&error);

    path = dbus_message_get_path (message);

    printf ("bailing out 1\n");
    if (path == NULL)
        goto out;

    for (n = client->domain_browsers; n != NULL; n = n->domain_browsers_next)
    {
        printf ("cmp: %s, %s\n", n->path, path);
        if (strcmp (n->path, path) == 0) {
            db = n;
            break;
        }
    }

    printf ("bailing out 2\n");
    if (db == NULL)
        goto out;

    dbus_message_get_args (message, &error, DBUS_TYPE_INT32, &interface,
            DBUS_TYPE_INT32, &protocol, DBUS_TYPE_STRING, &domain, DBUS_TYPE_INVALID);

    printf ("bailing out 3\n");
    if (dbus_error_is_set (&error))
        goto out;

    db->callback (db, interface, protocol, event, domain, db->user_data);

    return DBUS_HANDLER_RESULT_HANDLED;

out:
    dbus_error_free (&error);
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}
