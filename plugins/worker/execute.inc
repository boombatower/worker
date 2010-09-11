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
  'perform' => 'worker_execute_perform',
);

/**
 * Perform the job.
 *
 * @param $properties
 *   Associative array of properties defining the job.
 * @return
 *   Result of the command run.
 */
function worker_execute_perform(array $properties) {
  worker_log('Execute ' . $properties['command'] . '.');
  return worker_execute_output($properties['command']);
}
