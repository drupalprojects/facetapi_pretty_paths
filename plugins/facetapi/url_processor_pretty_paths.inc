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
   * Implements FacetapiUrlProcessor::fetchParams().
   *
   * Pulls facet params from the $_GET variable.
   */
  public function fetchParams() {
    $params = array('f' => array(), 'q' => $_GET['q']);
    $args = explode('/', $_GET['q']);
    $facets = $this->adapter->getEnabledFacets();
    while ($term = array_pop($args)) {
      $tid = array_pop(explode('-', $term));
      $vocab = array_pop($args);
      foreach ($facets as $alias => $facet) {
        if ($facet['label'] == $vocab) {
          $params['f'][] = $alias . ':' . $tid;
        }
      }
    }
    return $params;
  }

  /**
   * Implements FacetapiUrlProcessor::normalizeParams().
   *
   * Strips the "q" and "page" variables from the params array.
   */
  public function normalizeParams(array $params, $filter_key = 'f') {
    // @todo: interpret query path.
    return drupal_get_query_parameters($params, array('q', 'page'));
  }

  protected function getPathSegment($field, $value) {
    // Get term & vocabulary info fom field
    $field_info = field_info_field($field);
    if($field_info['type'] == 'taxonomy_term_reference') {

      $term = taxonomy_term_load($value);
      $voc = taxonomy_vocabulary_load($term->vid);

      return array(
        'name' => $voc->name,
        'value' => $term->name,
      );
    }
  }

  public function postProcessFacetBuild(array $facet, array &$build) {
    foreach ($build as $value => &$item) {
      $segments = $this->pathSegments;
      $tid = $item['#indexed_value'];

      // Add current facet to path if it is inactive.
      if (!$item['#active']) {
        $segments[$tid] = $this->getPathSegment($facet['field api name'], $tid);
      }
      // Remove current facet from url if it is active.
      else {
        unset($segments[$tid]);
      }

      // Modify paths "vocabulary/term-name-tid".
      foreach ($segments as $tid => $segment) {
        $path = '/' . $this->escapePath($segment['name']) .
          '/' . $this->escapePath($segment['value']) . '-' . $tid;

        $item['#path'] .= $path;
      }

      // Remove the un-pretty query.
      $item['#query'] = array();
    }
  }

  protected function escapePath($text) {
    if (module_exists('pathauto')) {
      // Needed, as of http://drupal.org/node/907578#comment-5564008
      require_once(drupal_get_path('module','pathauto').'/pathauto.inc');
      $text = pathauto_cleanstring($text);
      dpm($text);
    }
    else {
      $text = rawurlencode($text);
    }
    return $text;
  }

}
