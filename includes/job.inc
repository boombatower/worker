<?php
/**
 * @file
 * Provide convenience functions and environment setup for jobs.
 *
 * @author Jimmy Berry ("boombatower", http://drupal.org/user/214218)
 */

function worker_job_perform(array $properties) {
  if ($plugin = worker_plugins_get('worker', $properties['plugin'])) {
    worker_log('Starting job ' . $properties['item']->data['nid'] . '.' . $properties['item']->data['vid'] . '.' . $properties['item']->data['delta'] . '.');
    $result = $plugin['perform']($properties);
    worker_log('Job complete!');
    return $result;
  }
  return FALSE;
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
    $log_file = file_build_uri('job.log');
    file_put_contents($log_file, '');
  }

  // Write the log message with length of time since logging was started and
  // the message followed by a newline character.
  file_put_contents($log_file, sprintf("[%10s] %s\n", number_format(microtime(TRUE) - $start, 6), $string), FILE_APPEND);
}
