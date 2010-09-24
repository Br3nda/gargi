<?php

// This file is included from settings.php in both dev and dh-make-drupal deployed sites
// Put any settings that are shared between staging, prod and dev in here
$GLOBALS['simpletest_installed'] = TRUE;
if (preg_match("/^simpletest\d+$/", $_SERVER['HTTP_USER_AGENT'])) {
  $db_prefix = $_SERVER['HTTP_USER_AGENT'];
}
