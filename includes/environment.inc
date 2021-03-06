<?php
// $Id: environment.inc,v 1.8 2009/03/26 01:41:21 adrian Exp $

/**
 * @file
 * Functions used by drush to query the environment and
 * setting the current configuration.
 */

/**
 * The indicator for a Drupal installation folder.
 */
define('DRUSH_DRUPAL_BOOTSTRAP', 'includes/bootstrap.inc');

/**
 * @name Drush bootstrap phases
 * @{
 * Sequential Drush bootstrapping phases.
 */

/**
 * Only bootstrap Drush, without any Drupal specific code.
 *
 * Any code that operates on the Drush installation, and not specifically
 * any Drupal directory, should bootstrap to this phase.
 */
define('DRUSH_BOOTSTRAP_DRUSH', 0);

/**
 * Set up and test for a valid drupal root, either through the -r/--root options,
 * or evaluated based on the current working directory.
 *
 * Any code that interacts with an entire Drupal installation, and not a specific
 * site on the Drupal installation should use this bootstrap phase.
 */
define('DRUSH_BOOTSTRAP_DRUPAL_ROOT',  1);

/**
 * Set up a Drupal site directory and the correct environment variables to
 * allow Drupal to find the configuration file.
 *
 * If no site is specified with the -l / --uri options, Drush will assume the
 * site is 'default', which mimics Drupal's behaviour.
 *
 * If you want to avoid this behaviour, it is recommended that you use the
 * DRUSH_BOOTSTRAP_DRUPAL_ROOT bootstrap phase instead.
 *
 * Any code that needs to modify or interact with a specific Drupal site's
 * settings.php file should bootstrap to this phase.
 */
define('DRUSH_BOOTSTRAP_DRUPAL_SITE', 2);

/**
 * Load the settings from the Drupal sites directory.
 *
 * This phase is analagous to the DRUPAL_BOOTSTRAP_CONFIGURATION bootstrap phase in Drupal
 * itself, and this is also the first step where Drupal specific code is included.
 *
 * This phase is commonly used for code that interacts with the Drupal install API,
 * as both install.php and update.php start at this phase.
 */
define('DRUSH_BOOTSTRAP_DRUPAL_CONFIGURATION', 3);

/**
 * Connect to the Drupal database using the database credentials loaded
 * during the previous bootstrap phase.
 *
 * This phase is analogous to the DRUPAL_BOOTSTRAP_DATABASE bootstrap phase in
 * Drupal.
 *
 * Any code that needs to interact with the Drupal database API needs to
 * be bootstrapped to at least this phase.
 */
define('DRUSH_BOOTSTRAP_DRUPAL_DATABASE', 4);

/**
 * Fully initialize Drupal.
 *
 * This is the default bootstrap phase all commands will try to reach,
 * unless otherwise specified.
 * This is analogous to the DRUPAL_BOOTSTRAP_FULL bootstrap phase in
 * Drupal.
 *
 * Any code that interacts with the general Drupal API should be
 * bootstrapped to this phase.
 */
define('DRUSH_BOOTSTRAP_DRUPAL_FULL', 5);

/**
 * Log in to the initialiased Drupal site.
 *
 * This bootstrap phase is used after the site has been
 * fully bootstrapped.
 *
 * This phase will log you in to the drupal site with the username
 * or user ID specified by the --user/ -u option.
 *
 * Use this bootstrap phase for your command if you need to have access
 * to information for a specific user, such as listing nodes that might
 * be different based on who is logged in.
 */
define('DRUSH_BOOTSTRAP_DRUPAL_LOGIN', 6);


/**
 * Helper function listing phases.
 *
 * For commands that need to iterate through the phases, such as help
 */
function _drush_bootstrap_phases() {
  static $phases = array(
    DRUSH_BOOTSTRAP_DRUSH, DRUSH_BOOTSTRAP_DRUPAL_ROOT, 
    DRUSH_BOOTSTRAP_DRUPAL_SITE, DRUSH_BOOTSTRAP_DRUPAL_CONFIGURATION,
    DRUSH_BOOTSTRAP_DRUPAL_DATABASE, DRUSH_BOOTSTRAP_DRUPAL_FULL,
    DRUSH_BOOTSTRAP_DRUPAL_LOGIN,
  );

  return $phases;
}

/**
 * @} End of Drush bootstrap phases.
 */

/**
 * Bootstrap Drush to the desired phase.
 *
 * This function will sequentially bootstrap each
 * lower phase up to the phase that has been requested.
 *
 * @param phase
 *   The bootstrap phase to bootstrap to.
 *   Any of the following constants :
 *     DRUSH_BOOTSTRAP_DRUSH                = Only Drush.
 *     DRUSH_BOOTSTRAP_DRUPAL_ROOT          = Find a valid Drupal root.
 *     DRUSH_BOOTSTRAP_DRUPAL_SITE          = Find a valid Drupal site.
 *     DRUSH_BOOTSTRAP_DRUPAL_CONFIGURATION = Load the site's settings.
 *     DRUSH_BOOTSTRAP_DRUPAL_DATABASE      = Initialize the database.
 *     DRUSH_BOOTSTRAP_DRUPAL_FULL          = Initialize Drupal fully.
 *     DRUSH_BOOTSTRAP_DRUPAL_LOGIN         = Log into Drupal with a valid user.
 */

function drush_bootstrap($phase) {
  static $phases = array(
    DRUSH_BOOTSTRAP_DRUSH                  => '_drush_bootstrap_drush',
    DRUSH_BOOTSTRAP_DRUPAL_ROOT            => '_drush_bootstrap_drupal_root', 
    DRUSH_BOOTSTRAP_DRUPAL_SITE            => '_drush_bootstrap_drupal_site',
    DRUSH_BOOTSTRAP_DRUPAL_CONFIGURATION   => '_drush_bootstrap_drupal_configuration',
    DRUSH_BOOTSTRAP_DRUPAL_DATABASE        => '_drush_bootstrap_drupal_database',
    DRUSH_BOOTSTRAP_DRUPAL_FULL            => '_drush_bootstrap_drupal_full',
    DRUSH_BOOTSTRAP_DRUPAL_LOGIN           => '_drush_bootstrap_drupal_login');

  static $phase_index = 0;

  drush_set_context('DRUSH_BOOTSTRAPPING', TRUE);
  while ($phase >= $phase_index && isset($phases[$phase_index])) {
    drush_set_context('DRUSH_BOOTSTRAP_PHASE', $phase_index);
    $current_phase = $phases[$phase_index];
    if (function_exists($current_phase) && !drush_get_error()) {
      drush_log(dt("Drush bootstrap phase : !function()", array('!function' => $current_phase)), 'bootstrap');
      ob_start();
      $current_phase();
      ob_end_clean();
      
      if ($phase >= DRUSH_BOOTSTRAP_DRUPAL_FULL) {
        // We have enough of the system bootstrapped now to 
        // capture and integrate and drupal messages.
        _drush_log_drupal_messages();
      }
    }
    unset($phases[$phase_index++]);
  }
  drush_set_context('DRUSH_BOOTSTRAPPING', FALSE);
}

/**
 * Initial Drush bootstrap phase.
 *
 * During the initialization of Drush,
 * this is the first step where all we are
 * aware of is Drush itself.
 *
 * In this step we will register the shutdown function,
 * parse the command line arguments and store them in their
 * related contexts.
 *
 * Configuration files (drushrc.php) that are 
 *   a) Specified on the command line
 *   b) Stored in the root directory of drush.php
 *   c) Stored in the home directory of the system user.
 * 
 * Additionally the DRUSH_QUIET and DRUSH_BACKEND contexts,
 * will be evaluated now, as they need to be set very early in
 * the execution flow to be able to take affect/
 */ 
function _drush_bootstrap_drush() {
  // The bootstrap can fail silently, so we catch that in a shutdown function.
  register_shutdown_function('drush_shutdown');

  // parse the command line arguments.
  drush_parse_args();

  // Load a custom config specified with the --config option.
  drush_load_config('custom'); 

  // Load a drushrc.php file in the drush.php's directory.
  drush_load_config('drush');

  // Load a drushrc.php file in the ~/.drush directory.
  drush_load_config('user');

  $backend = drush_set_context('DRUSH_BACKEND', drush_get_option(array('b', 'backend')));

  if ($backend) {
    // Load options passed as a JSON encoded string through STDIN.
    $stdin_options = _drush_backend_get_stdin();
    if (is_array($stdin_options)) {
      drush_set_context('stdin', $stdin_options);
    }
  }
  $quiet = drush_set_context('DRUSH_QUIET', drush_get_option(array('q', 'quiet')));

  // When running in backend mode, all output is buffered, and returned
  // as a property of a JSON encoded associative array.
  if ($backend || $quiet) {
    ob_start();
  }

  drush_set_context('DRUSH_VERBOSE',     drush_get_option(array('v', 'verbose'), FALSE));
  drush_set_context('DRUSH_AFFIRMATIVE', drush_get_option(array('y', 'yes'), FALSE));
  drush_set_context('DRUSH_SIMULATE',    drush_get_option(array('s', 'simulate'), FALSE));

  // Find any command files that are available during this bootstrap phase.
  _drush_find_commandfiles(DRUSH_BOOTSTRAP_DRUSH);
}

/**
 * Bootstrap Drush with a valid Drupal Directory.
 *
 * In this function, the pwd will be moved to the root
 * of the Drupal installation.
 *
 * We also now load the drushrc.php for this specific platform.
 * We can now include files from the Drupal Tree, and figure
 * out more context about the platform, such as the version of Drupal.
 */
function _drush_bootstrap_drupal_root() {
  $drupal_root = drush_get_option(array('r', 'root'), drush_locate_root());

  if (!drush_valid_drupal_root($drupal_root)) {
    drush_set_error('DRUSH_INVALID_DRUPAL_ROOT', dt("The directory !drupal_root does not contain a valid Drupal installation", array('!drupal_root' => $drupal_root)));
  }
  else {
    drush_set_context('DRUSH_DRUPAL_ROOT', $drupal_root);
    chdir($drupal_root);
    
    define('DRUPAL_ROOT', $drupal_root);
    
    drush_load_config('drupal');

    require_once(DRUSH_DRUPAL_BOOTSTRAP);

    $version = drush_set_context('DRUSH_DRUPAL_VERSION', drush_drupal_version());
    $major_version = drush_set_context('DRUSH_DRUPAL_MAJOR_VERSION', drush_drupal_major_version());

    drush_log(dt("Initialized Drupal !version root directory at !drupal_root", array("!version" => $version, '!drupal_root' => $drupal_root)));
  }
}

/**
 * Initialize a site on the Drupal root.
 *
 * We will now check if we have a valid site that is specified for
 * this command.
 *
 * Sets up various constants and $_SERVER entries used by
 * Drupal, essentially mimicking a webserver environment.
 */
function _drush_bootstrap_drupal_site() {
  $drush_uri = drush_set_context('DRUSH_URI', 
      drush_get_option(array('l', 'uri'), drush_site_uri()));

  // Fake the necessary HTTP headers that Drupal needs:
  if ($drush_uri) {
    $drupal_base_url = parse_url($drush_uri);
    $_SERVER['HTTP_HOST'] = $drupal_base_url['host'];
    $_SERVER['PHP_SELF'] = $drupal_base_url['path'] . '/index.php';
  }
  else {
    $_SERVER['HTTP_HOST'] = 'default';
    $_SERVER['PHP_SELF'] = '/index.php';
  }

  $_SERVER['REQUEST_URI'] = $_SERVER['SCRIPT_NAME'] = $_SERVER['PHP_SELF'];
  $_SERVER['REMOTE_ADDR'] = '127.0.0.1';
  $_SERVER['REQUEST_METHOD']  = NULL;
  $_SERVER['SERVER_SOFTWARE'] = NULL;
  $_SERVER['HTTP_USER_AGENT'] = NULL;

  $site = drush_set_context('DRUSH_DRUPAL_SITE', $_SERVER['HTTP_HOST']);
  if (($conf_path = conf_path()) && !file_exists("./$conf_path/settings.php")) {
    drush_set_error('DRUPAL_SITE_SETTINGS_NOT_FOUND');
  }
  else {
    drush_set_context('DRUSH_DRUPAL_SITE_ROOT', $conf_path);

    drush_log(dt("Initialized Drupal site !site at !site_root", array('!site' => $site, '!site_root' => $conf_path)));
  }

  drush_load_config('site');
}

/**
 * Initialize and load the Drupal configuration files.
 */
function _drush_bootstrap_drupal_configuration() {
  drupal_bootstrap(DRUPAL_BOOTSTRAP_CONFIGURATION);
}

/**
 * Attempt to load the drupal database connection.
 */
function _drush_bootstrap_drupal_database() {
  drupal_bootstrap(DRUPAL_BOOTSTRAP_DATABASE);
}

/**
 * Attempt to load the full Drupal system
 */
function _drush_bootstrap_drupal_full() {
  drupal_bootstrap(DRUPAL_BOOTSTRAP_FULL);
  
  // Find any command files that are available during this bootstrap phase.
  _drush_find_commandfiles(DRUSH_BOOTSTRAP_DRUPAL_FULL);

}

/**
 * Log into the bootstrapped Drupal site with a specific
 * username or user id.
 */
function _drush_bootstrap_drupal_login() {
  $drush_user = drush_set_context('DRUSH_USER', drush_get_option(array('u', 'user'), 0));

  drush_drupal_login($drush_user);
}





/**
 * Returns the current working directory.
 *
 * TODO: Could cache result, but it isn't really expensive.
 */
function drush_cwd() {
  // We use PWD if available because getcwd() resolves symlinks, which
  // could take us outside of the Drupal root, making it impossible to find.
  $path = $_SERVER['PWD'];
  if (empty($path)) {
    $path = getcwd();
  }

  // Convert windows paths.
  $path = _drush_convert_path($path);

  return $path;
}

/**
 * Converts a Windows path (dir1\dir2\dir3) into a Unix path (dir1/dir2/dir3).
 */
function _drush_convert_path($path) {
  return str_replace('\\','/', $path);
}

/**
 * Returns parent directory.
 *
 * @param string
 *   Path to start from.
 *
 * @return string
 *   Parent path of given path.
 */
function _drush_shift_path_up($path) {
  if (empty($path)) {
    return FALSE;
  }
  $path = explode('/', $path);
  // Move one directory up.
  array_pop($path);
  return implode('/', $path);
}

/**
 * Like Drupal conf_path, but searching from beneath.
 * Allows proper site uri detection in site sub-directories.
 *
 * Essentially looks for a settings.php file.
 *
 * @param string
 *   Search starting path. Defaults to current working directory.
 *
 * @return
 *   Current site path (folder containing settings.php) or FALSE if not found.
 */
function drush_site_path($path = NULL) {
  static $site_path;
  
  if (!isset($site_path)) {
    $site_path = FALSE;

    $path = empty($path) ? drush_cwd() : $path;
    // Check the current path.
    if (file_exists($path . '/settings.php')) {
      $site_path = $path;
    }
    else {
      // Move up dir by dir and check each.
      while ($path = _drush_shift_path_up($path)) {
        if (file_exists($path . '/settings.php')) {
          $site_path = $path;
          break;
        }
      }
    }
    
    // Last resort: try from site root
    if (!$site_path) {
      if ($site_root = drush_locate_root()) {
        if (file_exists($site_root . '/sites/default/settings.php')) {
          $site_path = $site_root . '/sites/default';
        }
      }
    }
  }

  return $site_path;
}

/**
 * Attempts to determine site URI from the given path, using
 * a couple of heuristics.
 *
 * @param
 *   Starting path or current working directory.
 *
 * @return
 *   Site uri string or false if none was found.
 */
function drush_site_uri($path = NULL) {
  $uri = FALSE;

  if ($site_path = drush_site_path($path)) {
    // TODO: the method of including the settings.php file here is 
    // problematic, since it may contain code that will be executed.
    // An alternative approach would be to use a regexp for extracting
    // a possible $base_url setting.

    // Export the following settings.php variables to the global namespace.
    global $databases, $db_url, $db_prefix, $cookie_domain, $conf, $installed_profile, $update_free_access;
    // If the settings.php has a defined path we use the URI from that.
    include_once($site_path . '/settings.php');
    if (isset($base_url)) {
      $uri = $base_url;
    }
    else {
      // Alternatively we default to the name of the current directory, if it is not 'default'.
      $elements = explode('/', $site_path);
      $current = array_pop($elements);
      if ($current != 'default') {
        $uri = 'http://'. $current;
      }
    }
  }

  return $uri;
}

/**
 * Exhaustive depth-first search to try and locate the Drupal root directory.
 * This makes it possible to run drush from a subdirectory of the drupal root.
 *
 * @param
 *   Search start path. Defaults to current working directory.
 */
function drush_locate_root($path = NULL) {
  $drupal_root = FALSE;

  $path = empty($path) ? drush_cwd() : $path;
  // Check the start path.
  if (drush_valid_drupal_root($path)) {
    $drupal_root = $path;
  }
  else {
    // Move up dir by dir and check each.
    while ($path = _drush_shift_path_up($path)) {
      if (drush_valid_drupal_root($path)) {
        $drupal_root = $path;
        break;
      }
    }
  }

  return $drupal_root;
}

/**
 * Checks whether given path qualifies as a Drupal root.
 *
 * @param string
 *   Path to check.
 *
 * @return boolean
 *   True if given path seems to be a Drupal root, otherwise false.
 */
function drush_valid_drupal_root($path) {
  return !empty($path) && is_dir($path) && file_exists($path . '/' . DRUSH_DRUPAL_BOOTSTRAP);
}
