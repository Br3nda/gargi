<?php

/**
 * Install one or more modules (ignoring dependencies!)
 *
 * @param mixed $modules either a single string module name, or an array of module names
 * @param array $res your res array, by reference. We'll write in an update_sql()-esqe entry for each module
 * @return array the last array added to your &$res
 * @author Neil Bertram <neil@catalyst.net.nz>
 */
function cat_profile_install_modules($modules, &$res) {
  module_rebuild_cache(); // search for any new modules
  $modules = (array)$modules;
  foreach ($modules as $module) {
    drupal_install_modules(array($module));
    $res[] = array(
      'query' => 'Installing '.$module.' module',
      'success' => db_result(db_query("SELECT status FROM {system} WHERE type='module' AND name='%s'", $module))
    );
    module_enable(array($module));
  }
  return $res;
}

/**
 * Remove one or more modules (ignoring dependencies)
 *
 * @param mixed $modules either a single string module name, or an array of module names
 * @param array $res your res array, by reference. We'll write in an update_sql()-esqe entry for each module
 * @return array the last array added to your &$res
 * @author Neil Bertram <neil@catalyst.net.nz>
 */
function cat_profile_uninstall_modules($modules, &$res) {
  $modules = (array)$modules;
  foreach ($modules as $module) {
    module_disable(array($module));
    drupal_uninstall_module($module);
    $res[] = array(
      'query' => 'Uninstalling '.$module.' module',
      'success' => !db_result(db_query("SELECT status FROM {system} WHERE type='module' AND name='%s'", $module))
    );
  }
  return $res;
}

/**
 * Add a permission to a role as part of a site_profile update
 *
 * @param string $role the role name (not rid), eg. 'administrator'
 * @param string $permission the permission string to add, eg. 'administer site configuration'
 * @return array update_sql()-esqe output
 * @author Neil Bertram <neil@catalyst.net.nz>
 * @author Unknown other author
 */
function cat_profile_role_add_permission($role, $permission) {
  return _cat_profile_role_permission($role, $permission, 'ADD');
}

/**
 * Remove a permission from a role as part of a site_profile upate
 *
 * @param string $role the role name (not rid), eg. 'administrator'
 * @param string $permission the permission string to remove, eg. 'administer site configuration'
 * @return array update_sql()-esqe output
 * @author Neil Bertram <neil@catalyst.net.nz>
 * @author Unknown other author
 */
function cat_profile_role_remove_permission($role, $permission) {
  return _cat_profile_role_permission($role, $permission, 'REMOVE');
}

/**
 * Internal function called from cat_profile_role_add_permission() and cat_profile_role_remove_permission()
 *
 * @param string $role the role name to work with
 * @param string $permission the permission string to add/remove
 * @param string $action one of 'ADD' or 'REMOVE'
 * @return array update_sql()-esqe output
 * @author Neil Bertram <neil@catalyst.net.nz>
 * @author Unknown other author
 */
function _cat_profile_role_permission($role, $permission, $action='ADD') {
  $result = db_query("SELECT r.*, p.perm FROM {role} r INNER JOIN {permission} p ON r.rid = p.rid WHERE r.name='%s'", $role);
  $num_nodes = db_result(db_query("SELECT COUNT(*) FROM {role} r INNER JOIN {permission} p ON r.rid = p.rid WHERE r.name='%s'", $role));

  if (!$num_nodes) {
    db_query("INSERT INTO {permission} (rid,perm,tid) SELECT rid, '', 0 FROM {role} WHERE name = '%s'", $role);
    $result = db_query("SELECT r.*, p.perm FROM {role} r INNER JOIN {permission} p ON r.rid = p.rid WHERE r.name='%s'", $role);
  }

  $num_nodes = db_result(db_query("SELECT COUNT(*) FROM {role} r INNER JOIN {permission} p ON r.rid = p.rid WHERE r.name='%s'", $role));

  if ($num_nodes) {
    $fields = db_fetch_array($result);

    $rid = $fields['rid'];
    $perms_array = explode(',', $fields['perm']);

    foreach ($perms_array as &$perm) {
      $perm = trim($perm);
    }

    $perms_array = array_filter($perms_array);

    if ($action=='ADD' && !in_array ($permission, $perms_array)) {
        $perms_array[] = $permission;
    }
    elseif ($action=='REMOVE' && $key = array_search ($permission, $perms_array)) {
        unset ($perms_array[$key]);
    }

    asort ($perms_array);
    $perms_str = implode (', ', $perms_array);

    $stmt = sprintf ("UPDATE {permission} SET perm = '%s' WHERE rid=%d", $perms_str, $rid);
    db_query($stmt);

    $action_str = ($action == 'REMOVED') ? 'Removed' : 'Added';
    return array('success' => TRUE, 'query' => "$action_str permission '$permission' to role '$role'");
  }

  return array('success' => FALSE, 'query' => "Role '$role' not found");
}

/**
 * Create a user
 *
 * @deprecated Note: You can probably do this with user_save() just as effectively.
 * @param string $username their username
 * @param string $mail email address
 * @param string $pass_md5 hashed password
 * @param array $roles array of roles to give them (by name, not rid)
 * @return array the standard .install return array
 * @author Neil Bertram <neil@catalyst.net.nz>
 */
function cat_profile_create_user($username, $mail, $pass_md5, $roles = array()) {
  $res_arr = array();

  // do they exist?
  $res = db_query("SELECT uid FROM {users} WHERE name='%s'", $username);
  $uid = db_result($res);
  if (!$uid) {
    // create the user
    $user = user_save(new stdClass(), array (
          'name' => $username,
          'mail' => $mail,
          'mode' => '0',
          'sort' => '0',
          'threshold' => '0',
          'theme' => '',
          'signature' => '',
          'created' => time(),
          'access' => '0',
          'login' => '0',
          'status' => '1',
          'timezone' => ($username == 'root') ? null : 0,
          'language' => '',
          'picture' => '',
          'init' => $mail,
          'data' => 'a:0:{}',
    ));

    // set their password
    db_query("UPDATE {users} SET pass='%s' WHERE uid=%d", $pass_md5, $user->uid);
    $uid = $user->uid;

    $res_arr[] = array('success' => true, 'query' => 'Created new user '.$username);
  }

  // ensure they have the roles specified
  $roles = (array)$roles;
  foreach ($roles as $role_name) {
    $res = db_query("SELECT 1 FROM {users_roles} WHERE uid = %d AND rid = (SELECT rid FROM {role} WHERE name = '%s')", $uid, $role_name);
    if (!db_result($res)) {
      db_query("INSERT INTO {users_roles} (uid, rid) VALUES (%d, (SELECT rid FROM {role} WHERE name = '%s'))", $uid, $role_name);
      $res_arr[] = array('success' => true, 'query' => 'Gave role '.$role_name.' to '.$username);
    }
  }

  return $res_arr;
}

/**
 * Creates a new role
 *
 * @param string $role the role name
 * @return array the standard format .install array
 * @author Neil Bertram <neil@catalyst.net.nz>
 */
function cat_profile_create_role($role) {
  $res_arr = array();

  // does the role exist?
  $res = db_query("SELECT rid FROM {role} WHERE name = '%s'", $role);
  $rid = db_result($res);
  if (!$rid) {
    // need to create it
    db_query("INSERT INTO {role} (name) VALUES('%s')", $role);
    $res_arr[] = array('success' => true, 'query' => 'Created new role '.$role);
  }

  return $res_arr;
}

/**
 * Makes sure the provided role has all possible permissions
 *
 * @param string $role the role name (not rid)
 * @return array the standard .install array format
 * @author Neil Bertram <neil@catalyst.net.nz>
 */
function cat_profile_role_ensure_all_perms($role) {
  $res_arr = array();

  $permissions = array();
  foreach (module_list(true, false, false) as $module) {
    $module_perms = module_invoke($module, 'perm');
    if (is_array($module_perms)) {
      $permissions = array_merge($permissions, $module_perms);
    }
  }
  $permissions = array_unique($permissions);

  if (!$permissions) {
    drupal_set_message('No permissions discovered', 'error');
  }

  $res = db_query("SELECT rid FROM {role} WHERE name = '%s'", $role);
  $rid = db_result($res);

  if (!$rid) {
    $res_arr[] = array('success' => false, 'query' => 'Find role ID for '.$role);
    return $res_arr;
  }

  $perms_string = implode(', ', $permissions);
  $res = db_query("UPDATE {permission} SET perm = '%s' WHERE rid=%d", $perms_string, $rid);
  if (db_affected_rows($res) == 0) {
    db_query("INSERT INTO {permission} (rid,perm,tid) VALUES(%d,'%s',%d)", $rid, $perms_string, 0); 
  }
  $res_arr[] = array('success' => true, 'query' => 'Updated permission set to all perms for role '.$role);

  return $res_arr;
}


/**
 * Creates a new block entry
 *
 * @param string $module the module that provides the block
 * @param string $delta the delta of the block within that module
 * @param string $region the region to display the block in
 * @param string $theme the name of the theme to apply this to
 * @param int $weight the weight within the region
 * @param string $title override the default title of the block
 * @param array() $pages if set, restrict display to only these pages
 * @author Martyn Smith <martyn@catalyst.net.nz>
 */
function _cat_profile_create_block($module, $delta, $region, $theme, $weight = 0, $title = '<none>', $pages = array()) {
  db_query(
    "INSERT INTO {blocks} (title, status,weight,region,throttle,module,delta,theme,pages,visibility) VALUES ('%s', %d,%d,'%s',%d,'%s','%s','%s','%s', %d)",
    $title, 1, $weight, $region, 0, $module, $delta, $theme, !empty($pages) ? implode(',', $pages) : '', !empty($pages) ? 1 : 0
  );
}

/**
 * This function should be called from hook_install() in various modules
 * to ensure that the update_sql() function is available to them
 *
 * @author Neil Bertram <neil@catalyst.net.nz>
 */
function cat_profile_enable_updates() {
  if (!function_exists('update_sql')) {
    function update_sql($sql) {
      $args = func_get_args();
      array_shift($args);
      return cat_profile_update_sql($sql, $args);
    }
  }
}

/**
 * A drop-in replacement for update_sql(), but supports placeholders too
 *
 * @param string $query the SQL query
 * @param mixed ... arguments to the query
 * @see db_query()
 * @return array standard .install format
 * @author Neil Bertram <neil@catalyst.net.nz>
 */
function cat_profile_update_sql() {
  $args = func_get_args();
  $sql = array_shift($args);
  $res = db_query($sql, $args);
  return array('success' => $res != false, 'query' => $sql);
}

/**
 * Programmatically create CCK fields and types using the content copy module
 *
 * @deprecated content types are exportable via Features. See http://drupal.org/project/features
 *
 * @param $type string
 *  content type to create, defaults to new type, if type exists, only fields will be added
 * @param $macro array
 *  exported array from content types -> export. If file is not specified, macro will be used
 * @param $file string
 *  path to file containing content copy exported macro data structure. no escaping needed.
 *
 * @author David Thomas <davidt@catalyst.net.nz>
 */
function cat_profile_import_content_type($type = '<create>', $macro = '', $file = '') {
  if(!module_exists("content_copy")){
    drupal_set_message('Programmatically creating CCK fields requires the Content Copy module. Exiting.');
    return;
  }
  require_once drupal_get_path('module', 'content').'/includes/content.admin.inc';
  require_once drupal_get_path('module', 'node') .'/content_types.inc';

  $values = array();
  $values['type_name'] = $type;
  if($file){
    if(file_exists($file)){
      $values['macro'] = file_get_contents($file);
    }else{
      drupal_set_message(t('Unable to read %file for import. Exiting.', array('%file' => $file)));
      return;
    }
  }elseif($macro){
    $values['macro'] = $macro;
  }
  $form_state = array();
  $form_state['values'] = $values;
  //drupal_set_message('<pre>DEBUG: '.print_r($values['macro'],1).'</pre>');
  $errs = drupal_execute("content_copy_import_form", $form_state);
  content_clear_type_cache();

  if ($errs) {
    drupal_set_message(print_r($errs, true), 'error');
    return array('success' => false, 'query' => 'Attempted to import CCK type '.$macro.$file);
  }
  return array('success' => true, 'query' => 'Imported CCK type '.$macro.$file);
}

/**
 * Imports a Rule into the system
 *
 * @param string $file the path to the import file
 * @author Unknown
 */
function cat_profile_import_rule($file) {
  if (!module_exists('rules')) {
    drupal_set_message("Can't import rule unless the rules module is installed", 'error');
    return;
  }

  $rule = file_get_contents($file);
  if (!$rule) {
    drupal_set_message("Failed to import $file", 'error');
    return;
  }

  $form_state = array(
    'import' => $rule
  );
  $errs = drupal_execute('rules_admin_form_import', $form_state);
  if ($errs) {
    drupal_set_message('Rule import failed: '.print_r($errs), 'error');
  }
  else {
    drupal_set_message("Imported Rule from $file");
  }
}

/**
 * Installs a menu hierarchically. Expects an array that can be passed to menu_link_save(),
 * with a key 'children' containing any subentries
 *
 * @param array $menu associative nested array acceptable to menu_link_save(), with 'children' element
 * @param int $plid the parent item to start at
 * @return void
 * @todo This should return stuff.
 * @author Unknown
 */
function cat_profile_install_menu_links($menu, $plid) {
  foreach ( $menu as $item ) {
    $item['plid'] = $plid;
    menu_link_save ($item);
    $mlid = db_last_insert_id('{menu_links}', 'mlid');
    cat_profile_install_menu_links($item['children'], $mlid);
  }
}

/**
 * Returns a list of site_profile modules that are installed. Normally there's just one.
 * @return array an array of module names
 */
function _cat_profile_get_site_profiles() {
  $res = db_query("SELECT name FROM {system} WHERE status = 1 AND type = 'module' AND info LIKE '%\"cat_profile\"%'");
  $modules = array();
  while ($module = db_result($res)) {
    $modules[] = $module;
  }
  return $modules;
}

/**
 * Installs all types from *.cck.inc files inside your site profile
 *
 * @deprecated support in Features module.
 *
 * @return void
 * @todo should return something
 * @author David Thomas <davidt@catalyst.net.nz>
 */
function cat_profile_install_cck_types() {
  foreach (_cat_profile_get_site_profiles() as $module) {
    // get all *.cck.inc files in cck dir
    $included_types = glob( drupal_get_path('module', $module)."/cck/{*.cck.inc,*.CCK.INC}", GLOB_BRACE);
    if ($included_types) {
      // check to see if the content type already exists
      $types = node_get_types();

      // loop over filenames and import
      foreach ($included_types as $type) {
        // if the content type already exists, send it through to the import function
        if (preg_match('/([a-zA-Z0-9_]+)\.cck\.inc/', basename($type), $matches)) {
          if (array_key_exists($matches[1], $types)) {
            $content_type = $matches[1]; 
          } else {
            // otherwise try to create it.
            $content_type = '<create>';
          }
        }

        cat_profile_import_content_type($content_type, '', $type);
      }
    }
  }
}

function cat_profile_install_contexts() {
  foreach (_cat_profile_get_site_profiles() as $module) {
    // get all *.context.inc files in context dir
    $included_contexts = glob( drupal_get_path('module', $module)."/context/{*.context.inc,*.CONTEXT.INC}", GLOB_BRACE);

    if ($included_contexts) {
      // loop over filenames and include context
      foreach ($included_contexts as $included_context) {
        $context = new StdClass;
        // on error resume next
        @eval(file_get_contents($included_context));

        context_save($context);
      }
    }
  }
}

/**
 * Implementation of hook_imagecache_default_presets
 * include imagecache exported presets from imagecache dir
 *
 * @deprecated use Features module instead, where possible
 */
function _cat_profile_imagecache_default_presets() {
  $presets = array();
  foreach (_cat_profile_get_site_profiles() as $module) {
    // get all preset.inc files in views dir
    $default_presets = glob( drupal_get_path('module', $module)."/imagecache/{*.imagecache.inc,*.IMAGECACHE.INC}", GLOB_BRACE);
    if ($default_presets) {
      // loop over filenames and include preset
      foreach ($default_presets as $default_preset) {
        // on error resume next
        // preset is defined in file and auto added to $presets array
        @eval(file_get_contents($default_preset));
      }
    }
  }
  return $presets;
}

/**
 * A form called from a menu callback to export a variable and display it.
 * This is useful when you need to see a variable for cat_profile_variable_set()
 *
 * @author Neil Bertram <neil@catalyst.net.nz>
 */
function cat_profile_var_dump_form(&$form_state) {
  $exported = array();
  if (!empty($form_state['values']['variable'])) {
    $regex = '%'.str_replace('*', '.*', $form_state['values']['variable']).'%';
    global $conf;
    foreach ($conf as $k => &$v) {
      if (preg_match($regex, $k)) {
        $exported[$k] = var_export($v, true);
      }
    }
    drupal_set_message(t('@count matching variables', array('@count' => count($exported))));
  }

  $form['variable'] = array(
    '#type' => 'textfield',
    '#title' => t('Variable name'),
    '#description' => t('Enter the name of the variable you would like to dump. Hint: you can dump multiple variables using * as a wildcard'),
    '#default_value' => isset($form_state['values']['variable']) ? $form_state['values']['variable'] : null,
    '#required' => true
  );
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Dump')
  );
  $form['#rebuild'] = true;
  foreach ($exported as $k => &$v) {
    $form['exported'][$k] = array(
      '#type' => 'item',
      '#title' => check_plain($k),
      '#value' => '<pre>'.check_plain($v).'</pre>'
    );
  }

  return $form;
}
/*
function cat_profile_var_dump_form_submit(&$form, &$form_state) {
  $form['#rebuild'] = true;
  $form_state['storage']['variable'] = $form_state['values']['variable'];
}*/

/**
 * Set a new string override for t()
 *
 * Will replace any existing translation
 *
 * @param string $orig the string to match
 * @param string $new what to replace it with
 * @param string $lang the language code to replace it with
 * @return array standard update result
 * @author Neil Bertram <neil@catalyst.net.nz>
 */
function cat_profile_set_string_override($orig, $new, $lang = 'en') {
  $translations = variable_get('locale_custom_strings_'.$lang, array());
  $translations[$orig] = $new;
  variable_set('locale_custom_strings_'.$lang, $translations);
  return array('success' => true, 'query' => t('Set translation for "@orig" to "@new"', array('@orig' => $orig, '@new' => $new)));
}

/**
 * Sets a variable, checking that it sets correctly
 *
 * @deprecated user strongarm with features instead. http://drupal.org/project/strongarm.
 *
 * @param string $name the name of the variable to set
 * @param string $value the value of the variable
 * @return array standard .install format
 * @author Neil Bertram <neil@catalyst.net.nz>
 */
function cat_profile_variable_set($name, $value) {
  variable_set($name, $value);
  return array('query' => t('Set variable @name to  @value', array('@name' => $name, '@value' => print_r($value, true))), 'success' => (variable_get($name, 'random default!!#$$(*&') == $value));
}

/**
 * Deletes a variable
 *
 * @param string $name the name of the variable to delete
 * @return array standard .install format
 * @author Neil Bertram <neil@catalyst.net.nz>
 */
function cat_profile_variable_delete($name) {
  variable_del($name);
  return array('query' => t('Delete variable @name', array('@name' => $name)), 'success' => variable_get($name, 'random!!') == 'random!!');
}

/**
 * Return a row in a table back to its static state.
 *
 * @param string $table the name of the table to restore to.
 * @param array $row key/value pairs of information to store to the table.
 * @param array $keys the conditions which must stay static such as serial columns.
 * @author Josh Waihi <josh@catalyst.net.nz>
 */
function cat_profile_restore_row($table, $row, $keys = array()) {
  $schema = drupal_get_schema($table);

  // The table being restored must support cat_profile.
  if (!isset($schema['cat_profile_static_fields'])) {
    return FALSE;
  }
  $fields = array();
  foreach ($schema['cat_profile_static_fields'] as $field) {
    if (isset($row[$field])) {
      $fields[$field] = $row[$field];
    }
  }
  if (!is_array($keys)) {
    $keys = array($keys);
  }
  if (empty($keys) && isset($schema['primary key'])) {
    $keys = $schema['primary key'];
  }
  // If there are no keys then we can create an UPDATE query. Return FALSE.
  elseif (empty($keys)) {
    return FALSE;
  }
  $where = $values = array();
  foreach ($keys as $field) {
    $where[$field] = $field . ' = ' . db_type_placeholder($schema['fields'][$field]['type']);
    $values[$field] = $row[$field];
  }
  // If the record exist, update it.
  if (db_result(db_query('SELECT COUNT(*) FROM {' . $table . '} WHERE ' . implode(' AND ', $where), $values))) {
    $sets = array();
    $update_fields = array_diff_assoc($fields, $values);
    foreach ($update_fields as $field => $value) {
      $sets[] = $field . ' = ' . db_type_placeholder($schema['fields'][$field]['type']);
    }
    if (!empty($sets)) {
      $query = 'UPDATE {' . $table . '} SET ' . implode(', ', $sets) . ' WHERE ' . implode(' AND ', $where);
      return db_query($query, array_merge($update_fields, $values));
    }
    return TRUE;
  }
  // If not, create it.
  else {
    $placeholders = array();
    foreach ($fields as $field => $value) {
      $placeholders[] = db_type_placeholder($schema['fields'][$field]['type']);
    }
    $query = 'INSERT INTO {' . $table . '} (' . implode(', ', array_keys($fields)) . ') VALUES (' . implode(', ', $placeholders) . ')';
    return db_query($query, $fields) ? SAVED_NEW : FALSE;
  }
}

/**
 * Retrieve available static data fields from a table.
 */
function cat_profile_static_fields($table) {
  $schema = drupal_get_schema($table);
  return isset($schema['cat_profile_static_fields']) ? $schema['cat_profile_static_fields'] : array();
}

/**
 * Revert an exported data set.
 *
 * Make the generated data identical to $static_data.
 */
function cat_profile_data_revert($static_data, $table, $conditions) {
  foreach ($static_data as $table_name => $rows) {
    // Firstly, restore (UPDATE) or create (INSERT) any missing
    // or out dated rows.
    foreach ($rows as $row) {
      cat_profile_restore_row($table_name, $row);
    }
    // In PostgreSQL, we could potentially upset table
    // sequences so we should set them to there current
    // max value.
    cat_profile_update_psql_seqs($table_name);
  }

  $dynamic_data = cat_profile_export_table($table, $conditions);
  // Secondly, look through the dynamic data and find any added
  // rows that are not in the static rows. DELETE them.
  foreach ($dynamic_data as $table => $rows) {
    foreach ($rows as $r) {
      $remove = TRUE;
      foreach ($static_data[$table] as $row) {
        $diff = array_diff_assoc($r, $row);
        // If we find an identical row, then no need to remove it
        // and no need to continue looking.
        if (!($remove = !empty($diff))) break;
      }
      if ($remove) {
        $schema = drupal_get_schema($table);
        $conditions = array();
        foreach ($r as $key => $value) {
          $conditions[] = $key . ' = ' . db_type_placeholder($schema['fields'][$key]['type']);
        }
        db_query('DELETE FROM {' . $table . '} WHERE ' . implode(' AND ', $conditions), $r);
      }
    }
  }
}

/**
 * Export information from a table and its releated tables.
 */
function cat_profile_export_table($table, $conditions = array()) {
  $schema = drupal_get_schema($table);
  if (!isset($schema['cat_profile_static_fields'])) {
    return array();
  }
  $query = 'SELECT ' . implode(', ', $schema['cat_profile_static_fields']) . ' FROM {' . $table . '} %where% ORDER BY ' . implode(', ', $schema['cat_profile_static_fields']);
  if (!empty($conditions)) {
    $where = $args = array();
    foreach ($conditions as $key => $value) {
      if (!is_array($value)) {
        $value = array($value);
      }
      $where[] =  $key . ' IN (' . db_placeholders($value, $schema['fields'][$key]['type']) . ')';
      foreach ($value as $arg) {
        $args[] = $arg;
      }
    }
    $query = strtr($query, array('%where%' => 'WHERE ' . implode(' AND ', $where)));
  }
  else {
    $query = str_replace('%where%', '', $query);
  }
  $rs = db_query($query, $args);
  $data[$table] = array();
  $export = array();
  while ($row = db_fetch_array($rs)) {
    $data[$table][] = $row;
    if (!isset($schema['cat_profile_export'])) {
      continue;
    }
    foreach ($schema['cat_profile_export'] as $t => $column) {
      $export[$t][$column][] = $row[$column];
    }
  }
  foreach ($export as $table => $conditions) {
    $data += cat_profile_export_table($table, $conditions);
  }
  return $data;
}

/**
 * Reset PostgreSQL sequences.
 *
 * Cat profile may insert or update serial columns manually. Should this happen
 * the sequences will need to be updated to ensure no serial clashes occur
 * later on when addtional values are added.
 */
function cat_profile_update_psql_seqs($table) {
  global $db_type;
  // If this isn't a PostgreSQL system, then this function isn't needed.
  if ($db_type != 'pgsql') return TRUE;

  $schema = drupal_get_schema($table);
  foreach ($schema['fields'] as $field => $info) {
    if ($info['type'] != 'serial') continue;

    $seq = '{' . $table . '}_' . $field . '_seq';
    // We use SELECT setval() syntax because ALTER SEQUENCE 'foo' RESTART WITH
    // doesn't allow us to dynamically generate the sequence value.
    db_query("SELECT setval('$seq', (SELECT MAX($field) FROM {" . $table . "}), true)");
  }
}
