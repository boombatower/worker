<?php
/**
 * @file
 * Provide command execution job type worker plugin.
 *
 * @author Jimmy Berry ("boombatower", http://drupal.org/user/214218)
 */

$plugin = array(
  'title' => t('Execute'),
  'description' => t('Provides basic command execution.'),
  'execute' => 'worker_script_execute',
);

/**
 * Execute the job.
 *
 * @param $properties
 *   Associative array of properties defining the job.
 * @return
 *   Result of the command run
 */
function worker_script_execute(array $properties) {
  return worker_execute_output($properties['command']);
}
