<?php

/**
 * The Drupal AHAH framework won't execpt [, ], (, ), : or .
 * as characters in input name attributes, therefore, we need
 * to use a seperator that is compliant and doesn't conflict
 * with existing namespaces. For example, a block's unique
 * identifier is the theme, module and delta combine; for
 * features that needs to be one key so wee need to seperate
 * them with a seperator delimiter.
 */
define('CAT_PROFILE_FEATURES_DELIMITER', '----');

/**********
 *  Core  *
 *********/

/**
 * @ingroup blocks.
 * @{
 */

/**
 * Implementation of hook_features_export_options().
 * @author Josh Waihi <josh@catalyst.net.nz>
 */
function cat_profile_blocks_features_export_options()  {
  $rs = db_query("SELECT b.theme, b.module, b.delta FROM {blocks} b INNER JOIN {system} s ON s.name = b.theme AND s.type = 'theme' WHERE s.status=1 OR s.name ='%s'", variable_get('theme_default', 'garland'));
  $blocks = array();
  $module_blocks = array();
  while ($row = db_fetch_array($rs)) {
    if (!isset($module_blocks[$row['module']])) {
      $module_blocks[$row['module']] = module_invoke($row['module'],'block','list');
    }
    // $key = strtr('theme--s--module--s--delta', $row);
    $key = implode(CAT_PROFILE_FEATURES_DELIMITER, $row);
    $blocks[$key] = $row['theme'] . ': ' . $module_blocks[$row['module']][$row['delta']]['info'];
  }
  return $blocks;
}

/**
 * Implementation of hook_features_export().
 * @author Josh Waihi <josh@catalyst.net.nz>
 */
function cat_profile_blocks_features_export($data, &$export, $module_name = '') {
  foreach ($data as $key => $title) {
    $export['features']['cat_profile_blocks'][$key] = $title;
    list($theme, $module, $delta) = explode(CAT_PROFILE_FEATURES_DELIMITER, $key);
    $export['dependencies'][$module] = $module;
  }
  $export['dependencies'][] = 'block';
}

/**
 * Implementation of hook_features_export_render().
 * @author Josh Waihi <josh@catalyst.net.nz>
 */
function cat_profile_blocks_features_export_render($module_name, $data) {
  module_load_include('inc', 'cat_profile');
  $export = array();
  foreach ($data as $key) {
    list($theme, $module, $delta) = explode(CAT_PROFILE_FEATURES_DELIMITER, $key);
    $export = array_merge($export, cat_profile_export_table('blocks', array('theme' => $theme, 'module' => $module, 'delta' => $delta)));
  }
  $code = '  $data = ' . features_var_export($export, '  ') . ';' . PHP_EOL;
  $code .= '  return $data;';
  return array('cat_profile_blocks' => $code);
}

/**
 * Implementation of hook_features_revert().
 * @author Josh Waihi <josh@catalyst.net.nz>
 */
function cat_profile_blocks_features_revert($module) {
  $data = module_invoke($module, 'cat_profile_blocks');
  foreach ($data['blocks'] as $row) {
    cat_profile_data_revert($data, 'blocks', array('theme' => $row['theme'], 'module' => $row['module'], 'delta' => $row['delta']));
  }
}

/**
 * @} End of "ingroup blocks".
 */

/**
 * @ingroup roles
 * @{
 */

/**
 * Implementation of hook_features_export_options().
 * @author Josh Waihi <josh@catalyst.net.nz>
 */
function cat_profile_roles_features_export_options() {
  $roles = user_roles(TRUE);
  unset($roles[DRUPAL_AUTHENTICATED_RID]);
  return array_combine($roles, $roles);
}

/**
 * Implementation of hook_features_export().
 * @author Josh Waihi <josh@catalyst.net.nz>
 */
function cat_profile_roles_features_export($data, &$export, $module_name = '') {
  foreach ($data as $role) {
    $export['features']['cat_profile_roles'][$role] = $role;
  }
}

/**
 * Implementation of hook_features_export_render().
 * @author Josh Waihi <josh@catalyst.net.nz>
 */
function cat_profile_roles_features_export_render($module_name, $data) {
  $roles = $role_names = array();
  $result = db_query('SELECT r.rid, r.name, p.perm FROM {role} r LEFT JOIN {permission} p ON r.rid = p.rid WHERE r.name IN (' . db_placeholders($data, 'text') . ')', $data);
  // Compile role array:
  // Add a comma at the end so when searching for a permission, we can
  // always search for "$perm," to make sure we do not confuse
  // permissions that are substrings of each other.
  while ($role = db_fetch_object($result)) {
    $role_permissions[$role->name] = $role->perm .',';
    $role_names[$role->rid] = $role->name;
  }

  // Render role/permission overview:
  foreach (module_invoke_all('perm') as $perm) {
    foreach ($data as $name) {
      // Builds arrays for checked boxes for each role
      if (strpos($role_permissions[$name], $perm .',') !== FALSE) {
        $roles[$name][] = $perm;
      }
    }
  }

  $code = "  \$roles = ". features_var_export($roles, '  ') .";\n";
  $code .= '  return $roles;';

  $code2 = "  \$names = " . features_var_export($role_names, '  ') . ";\n";
  $code2 .= '   return $names;';
  return array(
    'cat_profile_perms' => $code,
    'cat_profile_role_names' => $code2,
  );
}

/**
 * Implementation of hook_features_revert().
 * @author Josh Waihi <josh@catalyst.net.nz>
 */
function cat_profile_roles_features_revert($module) {
  // Firstly all the required roles exist with right ids.
  $names = (array) module_invoke($module, 'cat_profile_role_names');
  $roles = user_roles(TRUE);
  foreach ($names as $rid => $name) {
    if (isset($roles[$rid]) && ($name != $roles[$rid])) {
      db_query("UPDATE {role} SET name = '%s' WHERE rid = %d", $rid, $name);
    }
    elseif (!isset($roles[$rid])) {
      cat_profile_create_role($name);
      // Make sure the roles match.
      if ($rid != db_result(db_query('SELECT rid FROM {role} WHERE name = \'%s\'', $name))) {
        db_query("UPDATE {role} SET rid = %d WHERE name = '%s'", $rid, $name);
        global $db_type;
        if ($db_type == 'pgsql') {
          db_query("ALTER SEQUENCE {role}_rid_seq RESTART WITH %d", $rid+1);
        }
      }
    }
  }

  // Next, revert the permissions foreach role.
  $perms = module_invoke($module, 'cat_profile_perms');
  if (empty($perms)) {
    return;
  }
  module_load_include('inc', 'cat_profile');
  $result = db_query('SELECT r.name, p.perm FROM {role} r LEFT JOIN {permission} p ON r.rid = p.rid WHERE r.name IN (' . db_placeholders($perms, 'text') . ')', array_keys($perms));
  // Compile role array:
  // Add a comma at the end so when searching for a permission, we can
  // always search for "$perm," to make sure we do not confuse
  // permissions that are substrings of each other.
  while ($role = db_fetch_object($result)) {
    $permissions[$role->name] = explode(', ', $role->perm);
  }

  $ret = array();
  foreach ($perms as $name => $perms) {
    // Firstly discover if the role exists.
    if (!isset($permissions[$name])) {
      $ret[] = cat_profile_create_role($name);
      foreach ($perms as $perm) {
        $ret[] = cat_profile_role_add_permission($name, $perm);
      }
    }
    else {
      foreach ($permissions[$name] as $perm) {
        if (!in_array($perm, $perms)) {
          $ret[] = cat_profile_role_remove_permission($name, $perm);
        }
      }
      foreach ($perms as $perm) {
        if (!in_array($perm, $permissions[$name])) {
          $ret[] = cat_profile_role_add_permission($name, $perm);
        }
      }
    }
  }
}

/**
 * @} End of "ingroup roles".
 */

/**
 * @ingroup taxonomy.
 * @{
 */

/**
 * Implementation of hook_features_export_options().
 * @author Elliot Pahl <elliot@catalyst.net.nz>
 * @author Josh Waihi <josh@catalyst.net.nz>
 */
function cat_profile_taxonomy_features_export_options() {
  $vocabs = array();
  foreach (taxonomy_get_vocabularies() as $vid => $vocab) {
    $vocabs[$vid] = $vocab->name;
  }
  return $vocabs;
}

/**
 * Implementation of hook_features_export().
 * @author Elliot Pahl <elliot@catalyst.net.nz>
 * @author Josh Waihi <josh@catalyst.net.nz>
 */
function cat_profile_taxonomy_features_export($data, &$export, $module_name = '') {
  foreach ($data as $vid) {
    $export['features']['cat_profile_taxonomy'][$vid] = $vid;
  }
  $export['dependencies'][] = 'taxonomy';
}

/**
 * Implementation of hook_features_export_render().
 * @author Elliot Pahl <elliot@catalyst.net.nz>
 * @author Josh Waihi <josh@catalyst.net.nz>
 */
function cat_profile_taxonomy_features_export_render($module_name, $data) {
  module_load_include('inc', 'cat_profile');
  $data = cat_profile_export_table('vocabulary', array('vid' => $data));
  $code = ' $data = ' . features_var_export($data, '  ') . ';' . PHP_EOL;
  $code .= ' return $data;';
  return array('cat_profile_taxonomy' => $code);
}

/**
 * Implementation of hook_features_revert().
 * @author Elliot Pahl <elliot@catalyst.net.nz>
 * @author Josh Waihi <josh@catalyst.net.nz>
 */
function cat_profile_taxonomy_features_revert($module) {
  $data = module_invoke($module, 'cat_profile_taxonomy');
  $vids = array();
  foreach ($data['vocabulary'] as $row) {
    $vids[] = $row['vid'];
  }
  cat_profile_data_revert($data, 'vocabulary', array('vid' => $vids));
}

/**
 * @} End of "ingroup taxonomy".
 */

/***********
 * Contrib *
 **********/

/**
 * @ingroup messaging.
 * @{
 */

/**
 * Implementation of hook_features_export_options().
 */
function cat_profile_messaging_features_export_options() {
  $rs = db_query('SELECT type, msgkey, method FROM {messaging_message_parts} ORDER BY type, method, msgkey');
  while ($row = db_fetch_array($rs)) {
    $key = implode(CAT_PROFILE_FEATURES_DELIMITER, $row);
    $options[$key] = $key;
  }
  return $options;
}

/**
 * Implementation of hook_features_export().
 */
function cat_profile_messaging_features_export($data, &$export, $module_name = '') {
  foreach ($data as $preset) {
    $export['features']['cat_profile_messaging'][$preset] = $preset;
  }
}

/**
 * Implementation of hook_features_export_render().
 */
function cat_profile_messaging_features_export_render($module_name = '', $data) {
  foreach ($data as $key) {
    $args = explode(CAT_PROFILE_FEATURES_DELIMITER, $key);
    $message = db_result(db_query("SELECT message FROM {messaging_message_parts} WHERE type = '%s' AND msgkey = '%s' AND method = '%s'", $args));
    $presets[$key] = $message;
  }
  $code = "  \$message_parts = ". features_var_export($presets, '  ') .";\n";
  $code .= '  return $message_parts;';
  return array('cat_profile_messaging' => $code);
}

/**
 * Implementation of hook_features_revert().
 */
function cat_profile_messaging_features_revert($module = 'messaging') {
  module_load_include('inc', 'messaging', 'messaging.admin');
  $defaults = module_invoke($module, 'messaging_features_store');
  if (empty($defaults)) {
    return;
  }
  $settings = array();
  foreach ($defaults as $key => $message) {
    list($type, $msgkey, $method) = explode(CAT_PROFILE_FEATURES_DELIMITER, $key);
    $settings[$type][$msgkey][$method] = $message;
  }
  foreach ($settings as $group => $messages) {
    $form_values = array(
      'values' => array(
        'group' => $group,
        'messages' => $messages,
      ),
    );
    messaging_admin_message_form_submit(array(), $form_values);
  }
}

/**
 * @} End of "ingroup messaging"
 */

/**
 * @ingroup workflow
 * @{
 */

/**
 * Implementation of hook_features_export_options().
 * @author Josh Waihi <josh@catalyst.net.nz>
 */
function cat_profile_workflow_features_export_options()  {
  $workflows = workflow_get_all();
  return $workflows;
}

/**
 * Implementation of hook_features_export().
 * @author Josh Waihi <josh@catalyst.net.nz>
 */
function cat_profile_workflow_features_export($data, &$export, $module_name = '') {
  foreach ($data as $wid) {
    $workflow = workflow_load($wid);
    $export['features']['cat_profile_workflow'][$wid] = $wid;
    $rs = db_query("SELECT type FROM {workflow_type_map} WHERE wid = %d", $wid);
    while ($type = db_result($rs)) {
      $pipe['variable'][] = 'worflow_' . $type;
    }
  }
  $export['dependencies'][] = 'workflow';
  if (isset($pipe)) {
    return $pipe;
  }
}

/**
 * Implementation of hook_features_export_render().
 * @author Josh Waihi <josh@catalyst.net.nz>
 */
function cat_profile_workflow_features_export_render($module_name, $data) {
  module_load_include('inc', 'cat_profile');
  $data = cat_profile_export_table('workflows', array('wid' => $data));
  $code = ' $data = ' . features_var_export($data, '  ') . ';' . PHP_EOL;
  $code .= ' return $data;';
  return array('cat_profile_workflow_data' => $code);
}

/**
 * Implementation of hook_features_revert().
 * @author Josh Waihi <josh@catalyst.net.nz>
 */
function cat_profile_workflow_features_revert($module) {
  $data = module_invoke($module, 'cat_profile_workflow_data');
  $wids = array();
  foreach ($data['workflows'] as $row) {
    $wids[] = $row['wid'];
  }
  cat_profile_data_revert($data, 'workflows', array('wid' => $wids));
}

/**
 * @} End of "ingroup workflow"
 */

