<?php

/**
 * @file
 * A custom URL processor for pretty paths.
 */

/**
 * Extension of FacetapiUrlProcessor.
 */
class FacetapiUrlProcessorPrettyPaths extends FacetapiUrlProcessorStandard {

  /**
   * An array of pretty path segments.
   *
   * @var array.
   */
  protected $pathSegments = array();

  /**
   * The adapter that uses this FacetapiUrlProcessor.
   *
   * @var FacetapiAdapter
   */
  protected $adapter = array();

  /**
   * Constructor, sets adapter.
   *
   * @param FacetapiAdapter $adapter
   *   The adapter that uses this FacetapiUrlProcessor.
   */
  function __construct(FacetapiAdapter $adapter) {
    $this->adapter = $adapter;
  }

  /**
   * Pulls facet params from the $_GET variable.
   *
   * Overrides FacetapiUrlProcessorStandard::fetchParams().
   */
  public function fetchParams() {
    $params = array();

    // Pretty paths are expected to be in form "search/url/segment1/segment2/".
    // @todo: Logic to remove /search/url part from args.
    $args = explode('/', $_GET['q']);

    // Skip pretty paths logic for admin pages, as
    // for pretty paths, we have to manipulate $_GET['q].
    // @todo: Fix this workaround?
    if ($args[0] == 'admin') {
      return $_GET;
    }

    $facets = $this->adapter->getEnabledFacets();

    // Traverse all segments "<alias>/<value>" from right to left (array_pop).
    while ($value = array_pop($args)) {
      $alias = array_pop($args);
      $found = FALSE;
      $alias = $this->decodePathSegmentAlias($alias);

      // Look for matching facet.
      foreach ($facets as $facet_alias => $facet) {
        $pretty_paths_alias = $this->getFacetPrettyPathsAlias($facet);

        // Add to params if alias from url matches alias from facet settings.
        if ($pretty_paths_alias == $alias) {
          $found = TRUE;
          $value = $this->decodePathSegmentValue($value, $facet);

          $params[] = rawurlencode($facet_alias) . ':' . $value;

          // "Copy" to prevent $segment['facet'] from being overridden.
          $my_facet = $facet;
          $segment = $this->getPathSegment($my_facet, $value);
          $this->pathSegments[$segment['key']] = $segment;
        }
      }
      // When no more facet path segments have been found,
      // we assume the rest of the url as the search basePath.
      if (!$found) {
        array_push($args, $alias, $value);
        break;
      }
    }

    // Mock default parameters for facetapi & related search modules.
    if (!empty($params)) {
      // Set search base path.
      $_GET['q'] = implode('/', $args);
      // Set query params.
      $_GET[$this->filterKey] = $params;
    }
    return $_GET;
  }

  /**
   *  Remove the filter query part from params and return it.
   *
   *  Overrides FacetapiUrlProcessorStandard::getQueryString().
   */
  public function getQueryString(array $facet, array $values, $active) {
    $params = $this->params;
    unset($params[$this->filterKey]);
    return $params;
  }

  /**
   *  Pretty paths will be generated as "search/url/segment1/segment2/".
   *
   *  By default, a segment will look like:
   *    "<alias>/<value>".
   *
   *  Overrides FacetapiUrlProcessorStandard::getFacetPath().
   */
  public function getFacetPath(array $facet, array $values, $active) {
    $segments = $this->pathSegments;
    $active_items = $this->adapter->getActiveItems($facet);

    // Adds to segments if inactive, removes if active.
    foreach ($values as $value) {
      $segment = $this->getPathSegment($facet, $value);
      if ($active && isset($active_items[$value])) {
        unset($segments[$segment['key']]);
      }
      elseif (!$active) {
        $segments[$segment['key']] = $segment;
      }
    }

    $base = $this->adapter->getSearchPath();
    $path = $this->constructPath($base, $segments);
    return $path;
  }

  /* ### Generic helpers ### */

  /**
   * Construct a path from a given base path and filter segments.
   */
  protected function constructPath($basePath, array $segments) {
    // Sort to avoid multiple urls with duplicate content.
    ksort($segments);
    $path = $basePath;
    foreach ($segments as $key => $segment) {
      $this->encodePathSegment($segment);
      $path .=  '/' . $segment['alias'] . '/' . $segment['value'];
    }
    return $path;
  }

  /**
   * Generate a path segement for a given facet + value.
   */
  protected function getPathSegment(&$facet, $value) {
    $pretty_paths_alias = $this->getFacetPrettyPathsAlias($facet);
    return array(
      'alias' => $pretty_paths_alias,
      'value' => $value,
      'facet' => &$facet,
      'key' => $pretty_paths_alias . "_" . $value,
    );
  }

  protected function encodePathSegment(array &$segment) {
    // Default: <alias>/<value>

    // @todo: Make this pluggable?
    // Taxonomy special case: <alias>/<term-name>-<term-id>
    $facet = $segment['facet'];
    if ($facet['field type'] == 'taxonomy_term') {
      if ($term = taxonomy_term_load($segment['value'])) {
        $voc = taxonomy_vocabulary_load($term->vid);
        $segment['value'] = $this->prettyPath($term->name) . '-' . $term->tid;
      }
    }

    $segment['value'] = str_replace('/', '%2F', $segment['value']);
    $segment['alias'] = rawurlencode($segment['alias']);
  }

  protected function decodePathSegmentAlias($alias) {
    return rawurldecode($alias);
  }

  protected function decodePathSegmentValue($value, array $facet) {
    // @todo: Make this pluggable?
    // Taxonomy special case: <alias>/<term-name>-<term-id>
    if ($facet['field type'] == 'taxonomy_term') {
      $value = array_pop(explode('-', $value));
    }

    $value = str_replace('%2F', '/', $value);
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

  /**
   * Returns the pretty_paths_alias for a given facet.
   *
   * If there is no custom pretty_paths_alias settings, in will default
   * to rawurlencode($facet['field alias']).
   */
  public function getFacetPrettyPathsAlias(array $facet) {
    $facet_settings = $this->adapter->getFacetSettingsGlobal($facet);
    // Path alias defaults to facet_name.
    return !empty($facet_settings->settings['pretty_paths_alias']) ?
      $facet_settings->settings['pretty_paths_alias'] : $facet['field alias'];
  }
}
