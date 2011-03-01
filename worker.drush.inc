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

  $items['worker-cron'] = array(
    'description' => 'If the worker is not busy then request a new job and spawn a worker and monitor process if a job is available.',
    'aliases' => array('wc'),
  );
  $items['worker-monitor'] = array(
    'description' => 'Start and monitor the worker process.',
    'aliases' => array('wm'),
  );
  $items['worker-perform'] = array(
    'description' => 'Perform a job as defined in the specified file.',
    'arguments' => array(
      'file' => 'PHP file containing the job properties array defined as $properties in the global namespace.',
    ),
    'options' => array(
      '--report' => 'Report the result to the conduit server.',
      '--summary' => 'Instead of the standard output print only a summary.',
    ),
    'aliases' => array('wp'),
  );
  $items['worker-requirements'] = array(
    'description' => 'Check the status of the worker requirements.',
    'aliases' => array('wr'),
    'options' => array(
      '--summary' => 'Instead of the standard output print only passed or failed.',
    ),
  );
  $items['worker-status'] = array(
    'description' => 'Print the status of the worker, either "busy" or "idle".',
    'aliases' => array('ws'),
  );

  return $items;
}

/**
 * Implements hook_drush_init().
 */
function worker_drush_init() {
  // When not displaying help information define constants.
  if (!func_get_args() && module_exists('worker')) {
    define('DRUSH_WORKER_JOB', file_directory_path() . '/job.php');
    define('DRUSH_WORKER_LOG', file_directory_path() . '/job.log');
    define('DRUSH_WORKER_ERROR', file_directory_path() . '/error.log');
    define('DRUSH_WORKER_MONITOR', 10);
  }
}

/**
 * Perform worker cron tasks.
 *
 * If the worker is not busy then request a new job and spawn a worker and monitor process if a job is available.
 *
 * Overview of the cron process:
 *
 * cron process
 * - check if worker is busy
 * - request job
 * - start monitor process
 *
 * monitor process (-1)
 * - start worker process
 * - monitor worker until complete
 *   - upload job.log diff's every 10 seconds
 * - check for fatal errors upon process termination and report them
 *
 * worker process (-2)
 * - peform job
 * - report result to conduit server
 */
function drush_worker_cron() {
  // If no 'monitor' process is running then continue.
  if (!drush_worker_status_busy()) {
    // If a job is available then store the properties array in a file, launch
    // the perform and monitor processes.
    worker_include('rest');
    if ($properties = worker_rest_claim()) {
      // Store the job information in file.
      file_put_contents(DRUSH_WORKER_JOB, '<?php $properties = ' . var_export($properties, TRUE) . ';');

      // Start the monitor process (-1) which will start the worker process.
      worker_include('job');
      drupal_static('worker_execute_background', -2);
      worker_execute_background('drush worker-monitor', 'drush worker-cron');
    }
    else {
      if (variable_get('worker_halt')) {
        // Check the length of uptime and shut the machine down if within the
        // last four minutes of an hour. Taking into account that machine
        // reservation time and boot time may take up to two minutes and
        // shutdown make take a minute or two this ensure the box stays within
        // whole hour time blocks.
        preg_match('/up\s+\d+:(\d+)/', current(worker_execute_output('uptime')), $match);
        if ($match[1] >= 56) {
          worker_execute_background('sudo halt');
        }
      }
    }
  }
}

/**
 * Start and monitor the worker process.
 */
function drush_worker_monitor() {
  worker_include('job');
  worker_include('rest');

  // Start the worker process (-2).
  drupal_static('worker_execute_background', -3);
  $number = worker_execute_background('drush worker-perform --report --summary ' . DRUSH_WORKER_JOB . ' 2> ' . DRUSH_WORKER_ERROR);

  // Monitor the worker until it has completed.
  while (!worker_execute_background_done($number)) {
    // @TODO Upload log diff.

    sleep(DRUSH_WORKER_MONITOR);
  }

  // If the worker crashed due to a fatal error report the error.
  if ($error = trim(file_get_contents(DRUSH_WORKER_ERROR))) {
    $properties = drush_worker_perform_load(DRUSH_WORKER_JOB);
    $result = array(
      'status' => WORKER_STATUS_SETUP,
      'result' => 'Worker crashed',
      'log' => $error,
      'duration' => drush_worker_duration(DRUSH_WORKER_JOB),
    );
    if (file_exists(DRUSH_WORKER_LOG)) {
      $result['log'] = trim(file_get_contents(DRUSH_WORKER_LOG)) . "\n\n" . str_repeat('=', 80) . "\n\n" . $result['log'];
    }
    worker_rest_result($properties['item'], $result);
  }
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
 * Perform a job as defined in the specified file.
 */
function drush_worker_perform($file) {
  // Load the file and access the $properties array.
  $properties = drush_worker_perform_load($file);

  // Perform job.
  worker_include('job');
  $result = worker_perform($properties);
  $result['duration'] = drush_worker_duration($file);

  if (drush_get_option('report')) {
    // Report the result to the conduit server.
    worker_include('rest');
    worker_rest_result($properties['item'], $result);
  }

  if (drush_get_option('summary')) {
    drush_print($result['status']);
  }
  else {
    drush_print_r($properties);
    drush_print_r($result);
  }
}

/**
 * Load a properties file.
 *
 * @param $file
 *   Path to properties file.
 * @return
 *   Array of properties.
 */
function drush_worker_perform_load($file) {
  global $properties;
  require $file;
  $properties['item'] = (object) $properties['item'];
  return $properties;
}

/**
 * Check the status of the worker requirements.
 */
function drush_worker_requirements() {
  worker_include('job');
  worker_include('rest');

  // Basic requirements.
  $requirements = array(
    'PHP executable path found (%s).' => worker_php_path(),
    'Drush is configured properly.' => TRUE,
    'Variable worker_url found.' => variable_get('worker_url'),
    'Variable worker_login found.' => variable_get('worker_login'),
    'Variable worker_category found.' => variable_get('worker_category'),
    'Successfully authenticated with conduit server.' => worker_rest_login(),
  );

  // Check database setup.
  global $databases;
  foreach (array_keys($databases) as $database) {
    if (preg_match('/^(?P<key>(?P<base>.*)_(?P<driver>.*))_stub$/m', $database, $match) && isset($database[$match['key']])) {
      $requirements["Database pair found for [{$match['base']}] with driver [{$match['driver']}]."] = TRUE;
    }
  }

  // Iterate over requirements to determine overall status and output messages.
  $pass = TRUE;
  foreach ($requirements as $message => $value) {
    $pass = $pass && $value;
    if (!drush_get_option('summary')) {
      drush_log(sprintf($message, $value), $value ? 'success' : 'error');
    }
  }

  // If only the summary was requested the output passed or failed.
  if (drush_get_option('summary')) {
    drush_print($pass ? 'passed' : 'failed');
  }
}

/**
 * Print the status of the worker, either "busy" or "idle".
 */
function drush_worker_status() {
  drush_print(drush_worker_status_busy() ? 'busy' : 'idle');
}

/**
 * Determine if the worker is busy.
 *
 * @return
 *   TRUE if the worker is busy, otherwise FALSE.
 */
function drush_worker_status_busy() {
  worker_include('job');
  return !worker_execute_background_done(-1);
}

/**
 * Get the length of time elapsed since the job information was retrieved.
 */
function drush_worker_duration($file) {
  return time() - filemtime($file);
}
