<?php

/**
 * @file
 * A default coder for pretty paths.
 */

/**
 * Default FacetApiPrettyPathsCoder.
 */
class FacetApiPrettyPathsCoderDefault {

  public function encodePathSegment(array $args) {
    // Default: <alias>/<value>
    $args['segment']['value'] = str_replace('/', '%2F', $args['segment']['value']);
    $args['segment']['alias'] = rawurlencode($args['segment']['alias']);
  }

  public function decodePathSegmentAlias(array $args) {
    $alias = $args['alias'];
    // We need to decode path aliases twice.
    $alias = rawurldecode($alias);
    $alias = rawurldecode($alias);
    return $alias;
  }

  public function decodePathSegmentValue(array $args) {
    $value = $args['value'];
    $value = str_replace('%2F', '/', rawurldecode($value));
    return $value;
  }

  /**
   * Convert a given text to a pretty path using pathauto, if available.
   */
  protected function prettyPath($text) {
    // @todo: Make this pluggable?
    if (module_exists('pathauto')) {
      // Needed, as of http://drupal.org/node/907578#comment-5564008
      require_once drupal_get_path('module', 'pathauto') . '/pathauto.inc';
      return pathauto_cleanstring($text);
    }
    return $text;
  }

}