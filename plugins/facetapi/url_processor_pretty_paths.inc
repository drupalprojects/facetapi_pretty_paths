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
    // @todo: How to get current search path?
    //$base_path = 'search/job';
    // @todo: use regex?
    //$query = substr($_GET['q'], strlen($base_path) < strlen($_GET['q']) ? strlen($base_path) + 1 : strlen($base_path));

    $query = $_GET['q'];
    $args = explode('/', $query);

    //@todo: add old params?
    $params = array('f' => array(), 'q' => $query);

    $facets = $this->adapter->getEnabledFacets();

    while ($value = array_pop($args)) {
      $alias = array_pop($args);

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

          $params['f'][] = $facet['field api name'] . ':' . $value;
        }
      }
    }
    return $params;
  }

  /**
   * Implements FacetapiUrlProcessor::getQueryString().
   */
  public function getQueryString(array $facet, array $values, $active) {
    $qstring = $this->params;
    $active_items = $this->adapter->getActiveItems($facet);

    // Appends to qstring if inactive, removes if active.
    foreach ($values as $value) {
      if ($active && isset($active_items[$value])) {
        // Add pretty path segment to segments array.
        $segment = $this->getPathSegment($facet, $value);
        $this->pathSegments = array_merge($this->pathSegments, array($segment['key'] => $segment));
        unset($qstring[$this->filterKey][$active_items[$value]['pos']]);
      }
      elseif (!$active) {
        $field_alias = rawurlencode($facet['field alias']);
        $qstring[$this->filterKey][] = $field_alias . ':' . $value;
      }
    }

    // Removes duplicates, resets array keys and returns query string.
    // @see http://drupal.org/node/1340528
    $qstring[$this->filterKey] = array_values(array_unique($qstring[$this->filterKey]));
    return array_filter($qstring);
  }

  public function postProcessFacetBuild(array $facet, array &$build) {
    foreach ($build as $value => &$item) {
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

      // Remove the un-pretty query.
      // @todo: This might remove fulltext search params...
      $item['#query'] = array();
    }
  }

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
