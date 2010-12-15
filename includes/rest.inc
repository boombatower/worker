<?php
/**
 * @file
 * Provide REST client interactions.
 *
 * @author Jimmy Berry ("boombatower", http://drupal.org/user/214218)
 */

/**
 * Claim an item from a category queue.
 *
 * @return
 *   An array of merged properties for the job and the queue item or FALSE.
 */
function worker_rest_claim() {
  if ($response = worker_rest_post('item/claim', array('category' => variable_get('worker_category')))) {
    if ($response->code == 200) {
      return json_decode($response->data, TRUE);
    }
    watchdog('worker', 'Failed to claim item: "@error".', array('@error' => $response->error), WATCHDOG_ERROR);
  }
  return FALSE;
}

/**
 * Store a result for an item.
 *
 * @return
 *   TRUE if successfull, otherwise FALSE.
 */
function worker_rest_result($item, $pass, $result, $log) {
  if ($response = worker_rest_post('item/result', array('item' => $item, 'pass' => $pass, 'result' => $result, 'log' => $log))) {
    if ($response->code == 200) {
      return json_decode($response->data);
    }
    watchdog('worker', 'Failed to communicate result to conduit: "@error".', array('@error' => $response->error), WATCHDOG_ERROR);
  }
  return FALSE;
}

/**
 * Perform a REST POST request.
 *
 * @param $path
 *   Relative path.
 * @param $data
 *   Associative array of data.
 * @param $session
 *   (Optional) Include session cookie if available.
 * @return
 *   Response object or FALSE.
 * @see drupal_http_request()
 */
function worker_rest_post($path, $data, $session = TRUE) {
  // Setup drupal_http_request() options.
  $options = array(
    'headers' => array('Content-Type' => 'application/x-www-form-urlencoded'),
    'method' => 'POST',
    'data' => http_build_query($data, '', '&'),
  );

  // If requested, attempt to include session cookie header.
  if ($session && ($session = worker_rest_session())) {
    $options['headers'] += $session;
  }

  // Perform HTTP request.
  $response = drupal_http_request(variable_get('worker_url', '') . "/$path", $options);

  // If the session cookie was requested and the request returned 401
  // (Unauthorized) then renew the session and perform the request again.
  if ($session && $response->code == 401) {
    watchdog('worker', 'Conduit session expired.');
    worker_rest_session(TRUE);
    $response = worker_rest_post($path, $data);
  }
  return $response;
}

/**
 * Retreive the active worker session.
 *
 * @param $reset
 *   (Optional) Reset the worker session.
 * @return
 *   Associative array containing a session cookie.
 */
function worker_rest_session($reset = FALSE) {
  $session = &drupal_static(__FUNCTION__, FALSE);

  if ($reset) {
    $session = FALSE;
    cache_clear_all('worker_session', 'cache');
  }

  if (!$session) {
    if ($cache = cache_get('worker_session')) {
      $session = $cache->data;
    }
    else {
      if ($session = worker_rest_login()) {
        cache_set('worker_session', $session);
      }
    }
  }

  return $session;
}

/**
 * Authenticate the worker with the conduit server and get a session.
 *
 * @return
 *   Associative array containing a session cookie, otherwise FALSE.
 * @see worker_rest_session()
 */
function worker_rest_login() {
  if (($response = worker_rest_post('user/login', variable_get('worker_login', array()), FALSE))) {
    if ($response->code == 200) {
      $session = json_decode($response->data);
      return array('Cookie' => $session->session_name . '=' . $session->sessid);
    }
    watchdog('worker', 'Failed to login to conduit: "@error".', array('@error' => $response->error), WATCHDOG_ERROR);
  }
  return FALSE;
}