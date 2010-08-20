<?php
/**
 * @file
 * Provide convenience functions and environment setup for jobs.
 *
 * @author Jimmy Berry ("boombatower", http://drupal.org/user/214218)
 */

/**
 * Root directory for job.
 */
define('JOB_ROOT', DRUPAL_ROOT . '/' . file_directory_path() . '/job');

/**
 * Perform the given job.
 *
 * @param $properties
 *   Associative array of job properties.
 * @return
 *   The result of the job or FALSE.
 */
function worker_perform(array $properties) {
  $result = FALSE;

  // Ensure that the job plugin can be loaded.
  if ($plugin = worker_plugins_get('worker', $properties['plugin'])) {
    worker_log('Starting job ' . $properties['item']->data['nid'] . '.' . $properties['item']->data['vid'] . '.' . $properties['item']->data['delta'] . '.');

    // Perform preparation and then job.
    if (worker_prepare($properties)) {
      // Ensure the working directory is set to the DRUPAL_ROOT and invoke the
      // plugin with the job properties.
      chdir(DRUPAL_ROOT);
      $result = $plugin['perform']($properties);
    }

    chdir(DRUPAL_ROOT);
    worker_log('Job exiting.');
  }
  return $result;
}

/**
 * Perform preparation needed before job runs.
 *
 * @param $properties
 *   Associative array of job properties.
 * @return
 *   TRUE if successfull, otherwise FALSE.
 */
function worker_prepare(array $properties) {
  // Ensure the job root directory is available.
  if (!worker_directory_prepare()) {
    worker_log('Failed to prepare job root directory.');
    return FALSE;
  }

  // If any version control repositories specified then process them.
  if (!empty($properties['vcs'])) {
    if (!worker_vcs($properties)) {
      return FALSE;
    }
  }

  return TRUE;
}

/**
 * Prepare the job root directory.
 *
 * @return
 *   TRUE if successfull, otherwise FALSE.
 */
function worker_directory_prepare() {
  // If job directory exists then clear it out.
  if (file_exists(JOB_ROOT)) {
    file_unmanaged_delete_recursive(JOB_ROOT);
  }

  // Create job directory and chdir() to it.
  $directory = JOB_ROOT;
  return file_prepare_directory($directory, FILE_CREATE_DIRECTORY);
}

/**
 * Process all version control repositories.
 *
 * @param $properties
 *   Associative array of job properties.
 */
function worker_vcs(array $properties) {
  // Cycle through all repositories until complete or an error is encountered.
  foreach ($properties['vcs'] as $path => $url) {
    // Load plugin appropriate for handlign scheme.
    $scheme = parse_url($url, PHP_URL_SCHEME);
    if ($vcs = worker_plugins_get('vcs', $scheme)) {
      // Ensure the path specified exists.
      $directory = JOB_ROOT . '/' . ltrim($path, './');
      file_prepare_directory($directory, FILE_CREATE_DIRECTORY);
      worker_chdir($path);

      // Checkout from the repository.
      worker_log('Checkout [' . $vcs['print']($url) . '] into [' . $path . '].');
      if (!$vcs['checkout']($url)) {
        return FALSE;
      }
    }
    else {
      worker_log('Unsupported scheme in [' . $url . '].');
      return FALSE;
    }
  }
  return TRUE;
}

/**
 * Apply a patch in the working directory.
 *
 * @param $patch
 *   Path to patch file.
 * @return
 *   TRUE if successfull, otherwise FALSE.
 */
function worker_patch_apply($patch) {
  return worker_execute('patch -p0 -i ' . escapeshellarg($patch));
}

/**
 * Get a list of all files modified by a patch.
 *
 * @param
 *   Path to patch file, which must be in the unified format.
 * @return
 *   List of files modified by the patch.
 */
function worker_patch_modified_files($patch) {
  // Load the patch file and use regular expression to find all files modified
  // by the patch.
  $contents = file_get_contents($patch);
  preg_match_all('/^[\+-]{3} (.*?)\s/m', $contents, $matches, PREG_SET_ORDER);

  $files = array();
  foreach ($matches as $match) {
    // Ignore reference to /dev/null which will occur when a file is being
    // added or removed.
    if ($match[1] != '/dev/null') {
      $files[$match[1]] = $match[1];
    }
  }
  return array_values($files);
}

/**
 * Wrapper for PHP exec() function.
 *
 * All commands executed on the worker should be run through this command or
 * worker_execute_output(). If the exit status is non-zero the occurance will
 * be logged.
 *
 * @param $command
 *   Command to execute.
 * @param $return_output
 *   (Optional) Return the output of the command instead of TRUE or FALSE.
 * @param $capture_stderr
 *   Capture the stderr as part of the output.
 * @return
 *   If successfull then TRUE or the output of the command, otherwise FALSE.
 */
function worker_execute($command, $return_output = FALSE, $capture_stderr = TRUE) {
  // Escape the incomming shell command.
  $command = escapeshellcmd($command);

  // Redirect the stderr to stdout so it is also captured.
  if ($capture_stderr) {
    $command .= ' 2>&1';
  }

  // Execute the command and capture the output and status.
  exec($command, $output, $status);

  // If that exit status is non-zero then log the occurance.
  if ($status != 0) {
    worker_log("The following command generated a non-zero exit status of $status:\n" .
      "$command\n" .
      ($output ? trim(implode("\n", $output)) : '<<no output>>'));
    return FALSE;
  }
  return $return_output ? $output : TRUE;
}

/**
 * Wrapper for PHP exec() function that return the output.
 *
 * @param $command
 *   Command to execute.
 * @param $capture_stderr
 *   Capture the stderr as part of the output.
 * @return
 *   If successfull then TRUE or the output of the command, otherwise FALSE.
 * @see worker_execute()
 */
function worker_execute_output($command, $capture_stderr = TRUE) {
  return worker_execute($command, TRUE, $capture_stderr);
}

/**
 * Wrapper for PHP chdir() function.
 *
 * @param $directory
 *   Directory within the job root to switch to.
 */
function worker_chdir($directory = '') {
  $directory = ltrim($directory, './');
  chdir(JOB_ROOT . '/' . $directory);
}

/**
 * Add the string to the log.
 *
 * @param $string
 *   Message string to log.
 */
function worker_log($string) {
  static $start, $log_file;

  // First time function is called then initialze the $start to the current
  // float version of microtime(), generate the $log_file uri and create a
  // blank log file.
  if (!$start) {
    $start = microtime(TRUE);
    $log_file = DRUPAL_ROOT . '/' . file_directory_path() . '/job.log';
    file_put_contents($log_file, '');
  }

  // Write the log message with length of time since logging was started and
  // the message followed by a newline character.
  file_put_contents($log_file, sprintf("[%10s] %s\n", number_format(microtime(TRUE) - $start, 6), $string), FILE_APPEND);
}
