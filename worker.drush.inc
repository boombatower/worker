<?php
/**
 * @file
 * Provide Drush integration.
 *
 * @author Jimmy Berry ("boombatower", http://drupal.org/user/214218)
 */

/**
 * Implements hook_drush_command().
 */
function worker_drush_command() {
  $items = array();

  $items['worker-perform'] = array(
    'description' => dt('Perform a job as defined in the specified file.'),
    'arguments' => array(
      'file' => dt('PHP file containing the job properties array defined as $properties in the global namespace.'),
    ),
    'aliases' => array('wp'),
  );

  return $items;
}

/**
 * Ensure the input file exists and is readable.
 */
function drush_worker_perform_validate($file = '') {
  if (!file_exists($file) || !is_readable($file)) {
    return drush_set_error('FILE', dt('The file must exist and be readable.'));
  }
}

/**
 * Invoke the worker using the properties array found in the file.
 */
function drush_worker_perform($file) {
  // Load the file and access the $properties array.
  global $properties;
  require $file;

  // To properly simulate  job request add an 'item' to be associated with.
  if (!isset($properties['item'])) {
    $properties['item'] = new stdClass();
    $properties['item']->item_id = 17;
    $properties['item']->data = array(
      'nid' => 18,
      'vid' => 19,
      'delta' => 20,
    );
  }

  // Print out properties array.
  drush_log(dt('Loaded properties array.'), 'ok');
  drush_print_r($properties);

  // Perform job and print resulting array.
  worker_include('job');
  drush_print_r(worker_perform($properties));
}
