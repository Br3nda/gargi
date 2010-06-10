<?php

/**
 * Implementation of hook_features_api().
 * @author Josh Waihi <josh@catalyst.net.nz>
 */
function cat_profile_features_api() {
  return array(
    'cat_profile_roles' => array(
      'name' => t('Roles'),
      'default_hook' => 'cat_profile_perms',
    ),
    'cat_profile_workflow' => array(
      'name' => t('Workflow'),
      'default_hook' => 'cat_profile_workflow_data',
    ),
    'cat_profile_data' => array(
      'default_hook' => 'cat_profile_data',
    ),
  );
}

/**
 * @ingroup data
 * @{
 */

/**
 * Implementation of hook_features_export_render().
 * @author Josh Waihi <josh@catalyst.net.nz>
 */
function cat_profile_data_features_export_render($module_name, $data) {
  $code = ' $data = array();' . PHP_EOL;
  foreach ($data as $module) {
    $code .= "  \$data += {$module_name}_cat_profile_{$module}_data();\n";
  }
  $code .= '  return $data;';
  return array('cat_profile_data' => $code);
}

/**
 * @} End of "ingroup data"
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
  $export['features']['cat_profile_data']['workflow'] = 'workflow';
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
