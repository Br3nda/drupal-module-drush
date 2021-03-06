<?php
// $Id: drush.inc,v 1.19 2009/03/26 04:36:23 adrian Exp $

/**
 * @file
 * The drush API implementation and helpers.
 */


/**
 * Dispatch a given set of commands.
 * Modules can add commands by implementing hook_drush_command().
 *
 * @param
 *
 */
function drush_dispatch($command = NULL) {
  $command = ($command) ? $command : drush_get_command();
  $return = FALSE;

  if ($command) {
    // Call the callback function of the active command.
    $return = call_user_func_array($command['callback'], $command['arguments']);
  }
  
  // prevent a '1' at the end of the output
  if ($return === TRUE) {
    $return = '';
  }

  return $return;
}


/**
 * Detects the version number of the current Drupal installation,
 * if any. Returns false if there is no current Drupal installation,
 * or it is somehow broken.
 *
 * This function relies on the presence of DRUPAL_ROOT/modules/system/system.module
 *
 * @return
 *   A string containing the version number of the current
 *   Drupal installation, if any. Otherwise, return false.
 */
function drush_drupal_version() {
  static $version = FALSE;

  if (!$version) {
    if ($drupal_root = drush_get_context('DRUSH_DRUPAL_ROOT')) {
      if (file_exists($drupal_root . '/modules/system/system.module')) {
        // We can safely include system.module as it contains defines and functions only.
        require_once($drupal_root . '/modules/system/system.module');
        // We just might be dealing with an early Drupal version (pre 4.7)
        if (defined('VERSION')) {
          $version = VERSION;
        }
      }
    }
  }
  return $version;
}

/**
 * Returns the Drupal major version number (5, 6, 7 ...)
 */
function drush_drupal_major_version() {
  $major_version = FALSE;
  if ($version = drush_drupal_version()) {
    $version_parts = explode('.', $version);
    if (is_numeric($version_parts[0])) {
      $major_version = (integer)$version_parts[0];
    }
  }
  return $major_version;
}

/**
 * Save a string to a temporary file. Does not depend on Drupal's API.
 *
 * @param string $data 
 * @return string
 *   A path to the file.
 */
function drush_save_data_to_temp_file($data) {
  static $fp;

  $fp = tmpfile();
  fwrite($fp, $data);
  $meta_data = stream_get_meta_data($fp);
  return $meta_data['uri'];
}


/**
 * Calls a given function, passing through all arguments unchanged.
 *
 * This should be used when calling possibly mutative or destructive functions
 * (e.g. unlink() and other file system functions) so that can be suppressed
 * if the simulation mode is enabled.
 *
 * @param $function
 *   The name of the function.
 * @return
 *   The return value of the function, or TRUE if simulation mode is enabled.
 */
function drush_op($function) {
  $args = func_get_args();
  array_shift($args); // Skip function name

  if (drush_get_context('DRUSH_VERBOSE') || drush_get_context('DRUSH_SIMULATE')) {
     drush_print("Calling $function(". implode(", ", $args) .')');
  }

  if (drush_get_context('DRUSH_SIMULATE')) {
    return true;
  }

  return call_user_func_array($function, $args);
}

/**
 * Rudimentary replacement for Drupal API t() function.
 *
 * @param string
 *   String to process, possibly with replacement item.
 * @param array
 *  An associative array of replacement items.
 *
 * @return
 *   The processed string.
 *
 * @see t()
 */
function dt($string, $args = NULL) {
  if (function_exists('t')) {
    return t($string, $args);
  }
  else {
    if (!empty($args)) {
      return strtr($string, $args);
    }
    else {
      return $string;
    }
  }
}

/**
 * This is called if no command or an unknown command is entered.
 */
function drush_usage() {
  $commands = func_get_args();
  drush_set_context('DRUSH_SHOWING_HELP', TRUE);
  
  if (empty($commands)) {
    $phases = _drush_bootstrap_phases(); 
    drush_print(dt('Usage: drush.php [options] <command> <command> ...'));
    drush_print();
    drush_print('Run "drush help [command]" to view command-specific help.');
    drush_print(dt('Options: '));
    foreach (drush_get_option_help() as $option => $description) {
      $rows[] = array($option, $description);
    }

    drush_print_table($rows, 2);
    drush_print();
    drush_print('Commands: ');

    $printed_rows = array();
    $phase_index = DRUSH_BOOTSTRAP_DRUSH;

    foreach ($phases as $phase_index) {
      if ($phase_index > drush_get_context('DRUSH_BOOTSTRAP_PHASE')) {
        drush_bootstrap($phase_index);
      }
      if (!drush_get_error()) {
        $commands = drush_get_commands();
        $rows = array();
        foreach($commands as $key => $command) {
          if (!array_key_exists($key, $printed_rows)) {
            $rows[$key] = array(sprintf("%-20s", $key), $commands[$key]['description']);
          }
        }
        drush_print_table($rows, 2);
        $printed_rows = array_merge($printed_rows, $rows);
      }
      else {
        break; 
      }
    }
    return;
  }
  else {
    return drush_show_help($commands);
  }

  drush_set_error('DRUSH_COMMAND_NOT_FOUND', dt('Invalid command !command.', array('!command' => implode(" ", $commands))));
}

/**
 * Get the available options for Drush for use by help page.
 *
 * @return
 *   An associative array containing the option definition as the key, and the description as the value,
 *   for each of the available options.
 */
function drush_get_option_help() {
  // TODO: Add a hook for this, to allow other modules to add their options
  $options['-r <path>, --root=<path>'] = dt("Drupal root directory to use (default: current directory)");
  $options['-l <uri> , --uri=<uri>']   = dt('URI of the drupal site to use (only needed in multisite environments)');
  $options['-v, --verbose']            = dt('Display all available output');
  $options['-q, --quiet']              = dt('Hide all output');
  $options['-y, --yes']                = dt("Assume 'yes' as answer to all prompts");
  $options['-s, --simulate']           = dt("Simulate all relevant actions (don't actually change the system)");
  $options['-i, --include']            = dt("A list of paths to search for drush commands");
  $options['-c, --config']             = dt("Specify a config file to use. See example.drushrc.php");
  $options['-u, --user']               = dt("Specify a user to login with. May be a name or a number.");
  $options['-b, --backend']            = dt("Hide all output and return structured data (internal use only).");
  return $options;
}

/**
 * Prints out the default drush help page.
 */
function drush_show_help($commands) {
  $phases = _drush_bootstrap_phases();

  $commandstring = implode(" ", $commands);

  foreach ($phases as $phase_index) {
    if ($phase_index > drush_get_context('DRUSH_BOOTSTRAP_PHASE')) {
      drush_bootstrap($phase_index);
    }
    if (!drush_get_error()) {
      $commands = drush_get_commands();
      if (array_key_exists($commandstring, $commands)) {
        $command = $commands[$commandstring];
        $help = drush_command_invoke_all('drush_help', 'drush:'. $commandstring);
        if (!empty($help)) {
          drush_print(wordwrap(implode("\n", $help), 80));
          drush_print();
          
          // TODO: Just let commands define their own sections?
          // Standardization need not be mandated?
          $sections = array(
            'examples' => 'Examples',
            'arguments' => 'Arguments',
            'options' => 'Options',
          );
          
          foreach ($sections as $key => $value) {
            if ($command[$key]) {
              drush_print(dt($value) . ':');
              foreach ($command[$key] as $name => $description) {
                $rows[] = array($name, dt($description));
              }
              drush_print_table($rows, 2);
              unset($rows);
              drush_print();
            }
          }
          return TRUE;
        }
        else {
          drush_print("No help available for command 'drush $commandstring'.");
          return TRUE;
        }
      }
    }
    else {
      break;
    }
  }
  return drush_set_error('DRUSH_COMMAND_NOT_FOUND', dt('Invalid command !command.', array('!command' => $commandstring)));
}

/**
 * Executes a shell command.
 * Output is only printed if in verbose mode.
 * Output is stored and can be retrieved using drush_shell_exec_output().
 * If in simulation mode, no action is taken.
 *
 * @param $cmd
 *   The command to execute.
 * @param $indent
 *   Indentation for the output (only for verbose mode).
 */
function drush_shell_exec($cmd, $indent = 0) {
  if (drush_get_context('DRUSH_VERBOSE') || drush_get_context('DRUSH_SIMULATE')) {
    drush_print('Executing: ' . $cmd, $indent);
  }

  if (drush_get_context('DRUSH_SIMULATE')) {
    return true;
  }

  exec($cmd . ' 2>&1', $output, $result);
  
  _drush_shell_exec_output_set($output);

  if (drush_get_context('DRUSH_VERBOSE')) {
    foreach ($output as $line) {
      drush_print($line, $indent + 2);
    }
  }

  // Exit code 0 means success.
  return ($result == 0);
}

/**
 * Stores output for the most recent shell command.
 * This should only be run from drush_shell_exec().
 *
 * @param $output
 *   The output of the most recent shell command.
 *   If this is not set the stored value will be returned.
 */
function _drush_shell_exec_output_set($output = FALSE) {
  static $stored_output;
  if (!$output) return $stored_output;
  $stored_output = $output;
}

/**
 * Returns the output of the most recent shell command as an array of lines.
 */
function drush_shell_exec_output() {
  return _drush_shell_exec_output_set();
}

/**
 * Exits with a message.
 * TODO: Exit with a correct status code.
 */
function drush_die($msg = NULL, $status = NULL) {
  die($msg ? "drush: $msg\n" : '');
}

/**
 * Prints a message.
 * @param $message
 *   The message to print.
 * @param $indent
 *    The indentation (space chars)
 */
function drush_print($message = '', $indent = 0) {
  print str_repeat(' ', $indent) . (string)$message . "\n";
}

/**
 * Prints a message, but only if verbose mode is activated.
 * Returns TRUE if in verbose mode, otherwise FALSE.
 */
function drush_verbose($msg = FALSE, $indent = 0) {
  if (!drush_get_context('DRUSH_VERBOSE')) {
    return FALSE;
  }
  if (drush_get_context('DRUSH_VERBOSE') && $msg === FALSE) {
    return TRUE;
  }

  print str_repeat(' ', $indent) . (string)$msg . "\n";
  return TRUE;
}

/**
 * Ask the user a basic yes/no question.
 *
 * @param $msg The question to ask
 * @return TRUE if the user entered 'y', FALSE if he entered 'n'
 */
function drush_confirm($msg, $indent = 0) {
  print str_repeat(' ', $indent) . (string)$msg . " (y/n): ";

  if (drush_get_context('DRUSH_AFFIRMATIVE')) {
    print "y\n";
    return TRUE;
  }
  while ($line = trim(fgets(STDIN))) {
    if ($line == 'y') {
      return TRUE;
    }
    if ($line == 'n') {
      return FALSE;
    }
    print str_repeat(' ', $indent) . (string)$msg . " (y/n): ";
  }
}

/**
 * Print a formatted table.
 * @param $rows
 *   The rows to print
 * @param $indent
 *   Indentation for the whole table
 * @param $header
 *   If TRUE, the first line will be treated as table
 *   header and therefore be underlined.
 */
function drush_print_table($rows, $indent = 0, $header = FALSE) {
  if (count($rows) == 0) {
    // Nothing to output.
    return;
  }

  $indent = str_repeat(' ', $indent);
  $format = _drush_get_table_row_format($rows);

  $header_printed = FALSE;
  foreach ($rows as $cols) {
    // Print the current line.
    print $indent . vsprintf($format, $cols) . "\n";
    // Underline the first row if $header is set to true.
    if (!$header_printed && $header) {
      $headers = array();
      foreach ($cols as $col) {
        $headers[] = str_repeat('-', strlen($col));
      }
      print $indent . trim(vsprintf($format, $headers)) . "\n";
      $header_printed = TRUE;
    }
  }
}

/**
 * Create the table row format string to be used in vsprintf().
 */
function _drush_get_table_row_format($table) {
  $widths = _drush_get_table_column_widths($table);
  foreach ($widths as $col_width) {
    $col_formats[] = "%-{$col_width}s";
  }
  $format = implode("\t", $col_formats);
  return $format;
}

/**
 * Calculate table column widths.
 */
function _drush_get_table_column_widths($table) {
  $widths = array();
  foreach ($table as $row => $cols) {
    foreach ($cols as $col => $value) {
      $old_width = isset($widths[$col]) ? $widths[$col] : 0;
      $widths[$col] = max($old_width, strlen((string)$value));
    }
  }
  return $widths;
}


/**
 * @defgroup logging Logging information to be provided as output.
 * @{
 *
 * These functions are primarily for diagnostic purposes, but also provide an overview of tasks that were taken
 * by drush.
 */

/**
 * Add a log message to the log history.
 *
 * @param message
 *   String containing the message to be logged.
 * @param type
 *   The type of message to be logged. Common types are 'warning', 'error', 'success' and 'notice'.
 *   A type of 'failed' can also be supplied to flag as an 'error'.
 *   A type of 'ok' or 'completed' can also be supplied to flag as a 'success'
 *   All other types of messages will be assumed to be notices.
 */
function drush_log($message, $type = 'notice', $error = null) {
  $log =& drush_get_context('DRUSH_LOG', array());
  $entry = array(
     'type' => $type, 
     'message' => $message, 
     'timestamp' => time()
   );
  $entry['error'] = $error;
  $log[] = $entry;
  return _drush_print_log($entry);
}

/**
 * Retrieve the log messages from the log history
 *
 * @return
 *   Entire log history
 */
function drush_get_log() {
  return drush_get_context('DRUSH_LOG', array());
}

/**
 * Display the log message
 *
 * By default, only warnings and errors will be displayed, if 'verbose' is specified, it will also display notices.
 *
 * @param
 *   The associative array for the entry.
 *
 * @return
 *   False in case of an error or failed type, True in all other cases.
 */
function _drush_print_log($entry) {
  $red = "\033[31m\033[1m[%s]\033[0m";
  $yellow = "\033[1;33m\033[1m[%s]\033[0m";
  $green = "\033[0;33m\033[1m[%s]\033[0m";

  $return = TRUE;
  switch ($entry['type']) {
    case 'warning' : 
      $type_msg = sprintf($yellow, $entry['type']);
      break;
    case 'failed' : 
    case 'error' : 
      $type_msg = sprintf($red, $entry['type']);
      $return = FALSE;
      break;
    case 'ok' :
    case 'completed' :
    case 'success' :
      $type_msg = sprintf($green, $entry['type']);
      break;
    default : 
      if (!drush_get_context('DRUSH_VERBOSE')) {
        // print nothing. exit cleanly.
        return TRUE;
      }
      $type_msg = sprintf("[%s]", $entry['type']);
      break;
  }
  // When running in backend mode, log messages are not displayed, as they will
  // be returned in the JSON encoded associative array.
  if (drush_get_context('DRUSH_BACKEND') || drush_get_context('DRUSH_SHOWING_HELP')) {
    return $return;
  }
  // Place the status message right aligned with the top line of the error message.
  $message = wordwrap($entry['message'], 128);
  $lines = explode("\n", $message);
  $lines[0] = sprintf("%-128s%7s", $lines[0], $type_msg);
  $message = implode("\n", $lines);
  drush_print($message);
  return $return;
}

/**
* Turn drupal_set_message errors into drush_log errors
*/
function _drush_log_drupal_messages() {
  if (function_exists('drupal_get_messages')) {

    $messages = drupal_get_messages();
    
    if (array_key_exists('error', $messages)) {
      //Drupal message errors.
      foreach ((array) $messages['error'] as $error) {
        if (preg_match('/^warning:/', $error)) {
          drush_log(ereg_replace('/^warning: /', '', $error), 'warning');
        }
        elseif (preg_match('/^user warning:/', $error)) {
          drush_set_error('DRUSH_DRUPAL_ERROR_MESSAGE', ereg_replace('/^user warning: /', '', $error));
        }
        else {
          drush_set_error('DRUSH_DRUPAL_ERROR_MESSAGE', $error);
        }
      }
    }
  }
}

/**
 * @} End of "defgroup logging".
 */

/**
* @name Error status definitions
* @{
* Error code definitions for interpreting the current error status.
* @see drush_set_error(), drush_get_error(), drush_get_error_log(), drush_cmp_error()
*/

/** The command was not successfully completed. This is the default error status. */
define('DRUSH_NOT_COMPLETED', 0);
/** The command completed succesfully. */
define('DRUSH_SUCCESS', 1);
/** The command could not be completed because the framework has specified errors that have occured. */
define('DRUSH_FRAMEWORK_ERROR', 2);

/**
 * @} End of "name Error status defintions".
 */

/**
 * @defgroup errorhandling Managing errors that occur in the Drush framework.
 * @{
 * Functions that manage the current error status of the Drush framework.
 *
 * These functions operate by maintaining a static variable that is a equal to the constant DRUSH_FRAMEWORK_ERROR if an
 * error has occurred.
 * This error code is returned at the end of program execution, and provide the shell or calling application with
 * more information on how to diagnose any problems that may have occurred.
 */

/**
 * Set an error code for the error handling system.
 *
 * @param error
 *   A text string identifying the type of error.
 *
 * @param message
 *   Optional. Error message to be logged. If no message is specified, hook_drush_help will be consulted, 
 *   using a key of 'error:MY_ERROR_STRING'.
 *
 * @return
 *   Always returns FALSE, to allow you to return with false in the calling functions,
 *   such as <code>return drush_set_error('DRUSH_FRAMEWORK_ERROR')</code>
 */
function drush_set_error($error, $message = null) {
  $error_code =& drush_get_context('DRUSH_ERROR_CODE', DRUSH_NOT_COMPLETED);
  $error_code = DRUSH_FRAMEWORK_ERROR;

  $error_log =& drush_get_context('DRUSH_ERROR_LOG', array());

  if (is_numeric($error)) {
    $error = 'DRUSH_FRAMEWORK_ERROR';
  }

  $message = ($message) ? $message : drush_command_invoke_all('drush_help', 'error:' . $error);

  if (is_array($message)) {
    $message = implode("\n", $message);
  }

  $error_log[$error][] = $message;
  drush_log(($message) ? $message : $error, 'error', $error);

  return FALSE;
}

/**
 * Return the current error handling status
 *
 * @return
 *   The current aggregate error status
 */
function drush_get_error() {
  return drush_get_context('DRUSH_ERROR_CODE', DRUSH_NOT_COMPLETED);
}

/**
 * Return the current list of errors that have occurred.
 *
 * @return
 *   An associative array of error messages indexed by the type of message.
 */
function drush_get_error_log() {
  return drush_get_context('DRUSH_ERROR_LOG', array());
}

/**
 * Check if a specific error status has been set.
 *
 * @param error
 *   A text string identifying the error that has occurred.
 * @return
 *   TRUE if the specified error has been set, FALSE if not 
 */
function drush_cmp_error($error) {
  $error_log = drush_get_error_log();
  
  if (is_numeric($error)) {
    $error = 'DRUSH_FRAMEWORK_ERROR';
  }

  return array_key_exists($error, $error_log);
}

/**
 * @} End of "defgroup errorhandling".
 */

