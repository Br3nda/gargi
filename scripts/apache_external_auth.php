#!/usr/bin/php
<?php
/**
 * This script (along with mod-authnz-external) allows you to use Drupal's
 * user DB to authenticate users in Apache.
 *
 * To use it:
 * 1) apt-get install libapache2-mod-authnz-external
 * 2) a2enmod authnz_external
 * 3) Add to your Apache config:
 *
 * DefineExternalAuth drupal-site-blah pipe /var/www/drupal-site-blah/scripts/apache_external_auth.php
 * <Directory /var/www/drupal-site-blah>
 *    AuthType Basic
 *    AuthBasicProvider external
 *    AuthExternal drupal-site-blah
 *    Require valid-user
 * </Directory>
 *
 * See http://code.google.com/p/mod-auth-external/ for more info
 *
 * @author Neil Bertram <neil@catalyst.net.nz>
*/

list($user, $pass) = file('php://stdin');
$user = trim($user);
$pass = trim($pass);

// bootstrap Drupal
if (!file_exists('./includes')) chdir(dirname(dirname(__FILE__)));
include_once './includes/bootstrap.inc';
drupal_bootstrap(DRUPAL_BOOTSTRAP_DATABASE);

echo "$user\n$pass\n";

$res = db_result(db_query("SELECT 1 FROM {users} WHERE name = '%s' OR mail = '%s' AND pass = '%s'", $user, $user, md5($pass)));
exit($res ? 0 : 1);
