#!/bin/sh
#
# Runs cron for a given installation of Drupal.
#
# Ensures that when the site is installed across many webservers, only one of
# them will run cron for any given run.
#
# This script is apart of the Catalyst distrobution of Acquia Drupal.
#

DOCROOT=$1
URL=$2
SITENAME=$3
DELAY=$4

# You can pass a delay to this script to offset it running from right on the
# minute boundary. Useful when many sites are installed on the same webserver
/bin/sleep $DELAY>/dev/null 2>&1

if [ ! -d /var/log/sitelogs/$SITENAME ]; then
    echo "Site $SITENAME does not appear to exist"
    exit 1
fi

# The full path to the lock file
LOCKFILE=/var/lib/sitedata/$SITENAME/cron.lock

# Time in hours after which a lock file is considered expired
LOCKFILEMAXTIME=30

# Remove the lock file if it's too old
find $LOCKFILE -mmin +$LOCKFILEMAXTIME 2>/dev/null | xargs --no-run-if-empty rm

# Exit if we can't immediately obtain a lock
/usr/bin/dotlockfile -r0 $LOCKFILE || exit;

# Run the job
(echo [`date`][`hostname`] Running cron for $SITENAME ;  $DOCROOT/scripts/drupal.sh --root $DOCROOT $URL) >> /var/log/sitelogs/$SITENAME/cron.log 2>&1

# Now the job is done, sleep for a little while, so if the job was claimed and
# executed by one webserver before another even tried, the job won't be run
# twice
/bin/sleep 5

# Release the lock
/usr/bin/dotlockfile -u $LOCKFILE
