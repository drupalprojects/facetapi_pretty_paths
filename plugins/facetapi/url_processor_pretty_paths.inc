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

  protected $adapter = array();

  function __construct($adapter) {
    $this->adapter = $adapter;
  }

  /**
   * Implements FacetapiUrlProcessor::fetchParams().
   *
   * Pulls facet params from the $_GET variable.
   */

  public function fetchParams() {
    $params = $_GET;

    // Pretty paths are expected to be in form "search/url/segment1/segment2/".
    // @todo: Logic to remove /search/url part from args.
    $args = explode('/', $_GET['q']);

    $facets = $this->adapter->getEnabledFacets();

    // Traverse all segments ("<alias>/<value>") from right to left using array_pop.
    while ($value = array_pop($args)) {
      $alias = array_pop($args);

      // Look for matching facet.
      foreach ($facets as $facet_alias => $facet) {
        $pretty_paths_alias = $this->getFacetPrettyPathsAlias($facet);

        // Add to params if alias from url matches alias from facet settings.
        if ($pretty_paths_alias == $alias) {

          // @todo: Make this pluggable?
          // Taxonomy special case: <alias>/<term-name>-<term-id>
          $field_info = field_info_field($facet['field api name']);
          if ($field_info['type'] == 'taxonomy_term_reference') {
            $value = array_pop(explode('-', $value));
          }

          $params['f'][] = $facet['field alias'] . ':' . $value;
        }
      }
    }
    return $params;
  }

  /**
   *  Returns the query string variables for a facet item.
   *
   *  Don't manipulate query params at all, as pretty paths are path-driven.
   *  Instead, calculate the resulting pathSegments and store them,
   *  so they can be processed in postProcessFacetBuild.
   *
   *  Implements FacetapiUrlProcessor::getQueryString().
   */
  public function getQueryString(array $facet, array $values, $active) {
    $this->calculatePathSegments($facet, $values, $active);
    return $this->params;
  }

  public function postProcessFacetBuild(array $facet, array &$build) {
    foreach ($build as $value => &$item) {
      $this->postProcessItem($facet, $item);
    }
  }

  /**
   * Post-processes a facet link item for pretty paths output.
   *
   * Rewrite its path, so it contains pretty facet paths,
   * but remove the default 'f=' query params.
   */
  protected function postProcessItem(array $facet, array &$item) {
    $segment = $this->getPathSegment($facet, $item['#indexed_value']);
    $path_segment = '/' . $segment['alias'] . '/' . $segment['value'];

    // Add current facet to path if it is inactive.
    if (!$item['#active']) {
      if (strpos($item['#path'], $path_segment) === FALSE) {
        $item['#path'] .= $path_segment;
      }
    }
    // Remove current facet from url if it is active.
    else {
      $pos = strpos($item['#path'], $path_segment);
      if ($pos !== FALSE) {
        $item['#path'] = str_replace($path_segment, "", $item['#path']);
      }
    }

    // Remove filter query params.
    $item['#query']['f'] = array();

    // Recursively postProcess children, if available.
    if (!empty($item['#item_children'])) {
      foreach ($item['#item_children'] as &$child) {
        $this->postProcessItem($facet, $child);
      }
    }
  }

  /**
   *  Calculate pathSegments and store them,
   *  so they can be processed in postProcessFacetBuild.
   */
  protected function calculatePathSegments(array $facet, array $values, $active) {
    $active_items = $this->adapter->getActiveItems($facet);
    foreach ($values as $value) {
      if ($active && isset($active_items[$value])) {
        // Add pretty path segment to segments array.
        $segment = $this->getPathSegment($facet, $value);
        $this->pathSegments = array_merge($this->pathSegments, array($segment['key'] => $segment));
      }
    }
  }

  /**
   * Generate a path segement for a given facet + value.
   */
  protected function getPathSegment($facet, $value) {
    $pretty_paths_alias = $this->getFacetPrettyPathsAlias($facet);

    // Default: <alias>/<value>
    $segment = array(
      'alias' => $pretty_paths_alias,
      'value' => $value,
    );

    // @todo: Make this pluggable?
    // Taxonomy special case: <alias>/<term-name>-<term-id>
    $field_info = field_info_field($facet['field api name']);
    if ($field_info['type'] == 'taxonomy_term_reference') {
      $term = taxonomy_term_load($value);
      $voc = taxonomy_vocabulary_load($term->vid);
      $segment['value'] = $this->prettyPath($term->name) . '-' . $term->tid;
    }

    $segment['key'] = $pretty_paths_alias . "_" . $value;
    return $segment;
  }

  /**
   * Convert a given text to a pretty path using pathauto, if available.
   */
  protected function prettyPath($text) {
    // @todo: Make this pluggable?
    if (module_exists('pathauto')) {
      // Needed, as of http://drupal.org/node/907578#comment-5564008
      require_once(drupal_get_path('module','pathauto').'/pathauto.inc');
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
      $facet_settings->settings['pretty_paths_alias'] : rawurlencode($facet['field alias']);
  }

}
