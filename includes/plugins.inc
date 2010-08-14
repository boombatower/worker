<?php
/**
 * @file
 * Provide ctools plugin definitions and implementation hooks.
 *
 * @author Jimmy Berry ("boombatower", http://drupal.org/user/214218)
 * @see http://panels3.dev.logrus.com/help/ctools/plugins-creating
 * @see http://panels3.dev.logrus.com/help/ctools/plugins-implementing
 */

/**
 * Fetch metadata on a specific plugin.
 *
 * @param $server
 *   Name of plugin.
 * @return
 *   An array of information about the requested plugin.
 */
function worker_plugins_get($type, $plugin) {
  ctools_include('plugins');
  return ctools_get_plugins('worker', $type, $plugin);
}

/**
 * Fetch metadata for all plugins.
 *
 * @return
 *   An array of arrays with information about all available plugins.
 */
function worker_plugins_get_all($type) {
  ctools_include('plugins');
  return ctools_get_plugins('worker', $type);
}

/**
 * Implements hook_ctools_plugin_TYPE().
 */
function worker_ctools_plugin_server() {
  return array(
    'cache' => TRUE,
  );
}

/**
 * Implements hook_ctools_plugin_directory().
 */
function worker_ctools_plugin_directory($module, $plugin) {
  if ($module == 'worker' && ($plugin == 'worker' || $plugin == 'vcs')) {
    return 'plugins/' . $plugin;
  }
}
