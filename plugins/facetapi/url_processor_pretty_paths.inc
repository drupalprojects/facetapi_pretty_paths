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
    $base_path = 'search/job';
    // @todo: use regex?
    $query = substr($_GET['q'], strlen($base_path) < strlen($_GET['q']) ? strlen($base_path) + 1 : strlen($base_path));
    $args = explode('/', $query);

    //@todo: add old params?
    $params = array('f' => array(), 'q' => $base_path);

    $facets = $this->adapter->getEnabledFacets();

    while ($value = array_pop($args)) {
      $alias = array_pop($args);

      foreach ($facets as $facet_alias => $facet) {
        $facet_settings = $this->adapter->getFacetSettingsGlobal($facet);
        $pretty_paths_alias = isset($facet_settings->settings['pretty_paths_alias']) ?
          $facet_settings->settings['pretty_paths_alias'] : $facet['field api name'];

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

  protected function getPathSegment($facet, $value) {
    $facet_settings = $this->adapter->getFacetSettingsGlobal($facet);
    $alias = !empty($facet_settings->settings['pretty_paths_alias']) ?
      $facet_settings->settings['pretty_paths_alias'] : $facet['field api name'];

    // Default: <alias>/<value>
    $segment = array(
      'alias' => $alias,
      'value' => $value,
    );

    // @todo: Make this pluggable?
    // Taxonomy special case: <alias>/<term-name>-<term-id>
    $field_info = field_info_field($facet['field api name']);
    if ($field_info['type'] == 'taxonomy_term_reference') {
      $term = taxonomy_term_load($value);
      $voc = taxonomy_vocabulary_load($term->vid);
      $segment['value'] = $term->name . '-' . $term->tid;
    }

    $segment['key'] = $alias . "_" . $value;
    return $segment;
  }

  public function postProcessFacetBuild(array $facet, array &$build) {
    foreach ($build as $value => &$item) {
      $segments = $this->pathSegments;
      $segment = $this->getPathSegment($facet, $item['#indexed_value']);

      // Add current facet to path if it is inactive.
      if (!$item['#active']) {
        $segments = array_merge($segments, array($segment['key'] => $segment));
        // Modify paths "alias/value".
        foreach ($segments as $key => $segment) {
          // @todo: doc rawurldecode vs escapePath
          $item['#path'] = '/' . rawurldecode($segment['alias']) .
            '/' . $this->escapePath($segment['value']);
        }
      }
      // Remove current facet from url if it is active.
      else {
        unset($segments[$value]);
      }
      // Remove the un-pretty query.
      // @todo: This might remove fulltext search params...
      $item['#query'] = array();
    }
  }

  protected function escapePath($text) {
    if (module_exists('pathauto')) {
      // Needed, as of http://drupal.org/node/907578#comment-5564008
      require_once(drupal_get_path('module','pathauto').'/pathauto.inc');
      $text = pathauto_cleanstring($text);
    }
    else {
      $text = rawurlencode($text);
    }
    return $text;
  }

}
