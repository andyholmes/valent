#!/bin/sh

COMMAND=json-schema-generate
ARGS="--format c"

if ! command -v ${COMMAND} &> /dev/null; then
    echo "${COMMAND} could not be found";
    exit 1;
fi

# Battery Plugin
${COMMAND} --format c \
           --c-variable-name battery_fuzz \
           data/schemas/kdeconnect.battery.json > \
           plugins/battery/kdeconnect.battery-fuzz.h

# Clipboard Plugin
${COMMAND} --format c \
           --c-variable-name clipboard_fuzz \
           data/schemas/kdeconnect.clipboard.json > \
           plugins/clipboard/kdeconnect.clipboard-fuzz.h

${COMMAND} --format c \
           --c-variable-name clipboard_connect_fuzz \
           data/schemas/kdeconnect.clipboard.connect.json > \
           plugins/clipboard/kdeconnect.clipboard.connect-fuzz.h

# Contacts Plugin
${COMMAND} --format c \
           --c-variable-name contacts_request_all_uids_timestamps_fuzz \
           data/schemas/kdeconnect.contacts.request_all_uids_timestamps.json > \
           plugins/contacts/kdeconnect.contacts.request_all_uids_timestamps-fuzz.h

${COMMAND} --format c \
           --c-variable-name contacts_request_vcards_by_uid_fuzz \
           data/schemas/kdeconnect.contacts.request_vcards_by_uid.json > \
           plugins/contacts/kdeconnect.contacts.request_vcards_by_uid-fuzz.h

${COMMAND} --format c \
           --c-variable-name contacts_response_uids_timestamps_fuzz \
           data/schemas/kdeconnect.contacts.response_uids_timestamps.json > \
           plugins/contacts/kdeconnect.contacts.response_uids_timestamps-fuzz.h

${COMMAND} --format c \
           --c-variable-name contacts_response_vcards_fuzz \
           data/schemas/kdeconnect.contacts.response_vcards.json > \
           plugins/contacts/kdeconnect.contacts.response_vcards-fuzz.h

# Find My Phone Plugin
${COMMAND} --format c \
           --c-variable-name findmyphone_request_fuzz \
           data/schemas/kdeconnect.findmyphone.request.json > \
           plugins/findmyphone/kdeconnect.findmyphone.request-fuzz.h

# Lcok Plugin
${COMMAND} --format c \
           --c-variable-name lock_fuzz \
           data/schemas/kdeconnect.lock.json > \
           plugins/lock/kdeconnect.lock-fuzz.h

${COMMAND} --format c \
           --c-variable-name lock_request_fuzz \
           data/schemas/kdeconnect.lock.request.json > \
           plugins/lock/kdeconnect.lock.request-fuzz.h

# Mousepad Plugin
${COMMAND} --format c \
           --c-variable-name mousepad_echo_fuzz \
           data/schemas/kdeconnect.mousepad.echo.json > \
           plugins/mousepad/kdeconnect.mousepad.echo-fuzz.h

${COMMAND} --format c \
           --c-variable-name mousepad_keyboardstate_fuzz \
           data/schemas/kdeconnect.mousepad.keyboardstate.json > \
           plugins/mousepad/kdeconnect.mousepad.keyboardstate-fuzz.h

${COMMAND} --format c \
           --c-variable-name mousepad_request_fuzz \
           data/schemas/kdeconnect.mousepad.request.json > \
           plugins/mousepad/kdeconnect.mousepad.request-fuzz.h

# MPRIS Plugin
${COMMAND} --format c \
           --c-variable-name mpris_fuzz \
           data/schemas/kdeconnect.mpris.json > \
           plugins/mpris/kdeconnect.mpris-fuzz.h

${COMMAND} --format c \
           --c-variable-name mpris_request_fuzz \
           data/schemas/kdeconnect.mpris.request.json > \
           plugins/mpris/kdeconnect.mpris.request-fuzz.h

# Notification Plugin
${COMMAND} --format c \
           --c-variable-name notification_fuzz \
           data/schemas/kdeconnect.notification.json > \
           plugins/notification/kdeconnect.notification-fuzz.h

${COMMAND} --format c \
           --c-variable-name notification_action_fuzz \
           data/schemas/kdeconnect.notification.action.json > \
           plugins/notification/kdeconnect.notification.action-fuzz.h

${COMMAND} --format c \
           --c-variable-name notification_reply_fuzz \
           data/schemas/kdeconnect.notification.reply.json > \
           plugins/notification/kdeconnect.notification.reply-fuzz.h

${COMMAND} --format c \
           --c-variable-name notification_request_fuzz \
           data/schemas/kdeconnect.notification.request.json > \
           plugins/notification/kdeconnect.notification.request-fuzz.h

# Photo Plugin
${COMMAND} --format c \
           --c-variable-name photo_fuzz \
           data/schemas/kdeconnect.photo.json > \
           plugins/photo/kdeconnect.photo-fuzz.h

${COMMAND} --format c \
           --c-variable-name photo_request_fuzz \
           data/schemas/kdeconnect.photo.request.json > \
           plugins/photo/kdeconnect.photo.request-fuzz.h

# Share Plugin
${COMMAND} --format c \
           --c-variable-name share_request_fuzz \
           data/schemas/kdeconnect.share.request.json > \
           plugins/share/kdeconnect.share.request-fuzz.h

# Ping Plugin
${COMMAND} --format c \
           --c-variable-name ping_fuzz \
           data/schemas/kdeconnect.ping.json > \
           plugins/ping/kdeconnect.ping-fuzz.h
           
