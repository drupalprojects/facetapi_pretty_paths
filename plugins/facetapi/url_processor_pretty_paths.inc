<?php

/**
 * @file
 * A custom URL processor for pretty paths.
 */

/**
 * Extension of FacetapiUrlProcessor.
 */
class FacetapiUrlProcessorPrettyPaths extends FacetapiUrlProcessor {

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
    return $_GET;
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

  /**
   * Implements FacetapiUrlProcessor::getQueryString().
   */
  public function getQueryString(array $facet, array $values, $active) {
    $qstring = $this->params;
    $active_items = $this->adapter->getActiveItems($facet);

    // Appends to qstring if inactive, removes if active.
    foreach ($values as $value) {
      if ($active && isset($active_items[$value])) {
        $this->pathSegments[$value] = $this->getPathSegment($facet['field alias'], $value);
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

  /**
   * Implements FacetapiUrlProcessor::setBreadcrumb().
   */
  public function setBreadcrumb() {
    $breadcrumb = drupal_get_breadcrumb();

    // Gets search keys and active items form the adapter.
    $keys = $this->adapter->getSearchKeys();
    $active_items = $this->adapter->getAllActiveItems();

    $item = menu_get_item();
    $last_load_func = is_array($item['load_functions']) ? end($item['load_functions']) : NULL;
    if (!empty($item['title']) && ((!$keys && $active_items) || ($keys && $last_load_func != 'menu_tail_load'))) {
      $last = end($breadcrumb);
      $this_page = l($item['title'], $item['href'], $item['localized_options']);
      if ($last != $this_page) {
        $breadcrumb[] = $this_page;
      }
    }

    // Initializes base breadcrumb query.
    $query = $this->params;
    unset($query[$this->filterKey]);

    // Adds the current search to the query.
    if ($keys) {
      // The last item should be text, not a link.
      $breadcrumb[] = $active_items ? l($keys, $_GET['q'], array('query' => $query)) : check_plain($keys);
    }

    // Adds filters to the breadcrumb trail.
    $last = end($active_items);
    foreach ($active_items as $item) {
      $query[$this->filterKey][] = $item['field alias'] . ':' . $item['value'];

      // Replaces with the mapped value.
      $value = $this->adapter->getMappedValue($item['facets'][0], $item['value']);

      // The last item should be text, not a link.
      if ($last == $item) {
        $breadcrumb[] = !empty($value['#html']) ? $value['#markup'] : check_plain($value['#markup']);
      }
      else {
        // Appends the filter to the breadcrumb trail.
        $breadcrumb[] = l($value['#markup'], $_GET['q'], array('query' => $query, 'html' => !empty($value['#html'])));
      }
    }

    // Sets the breadcrumb trail with h keys and filters.
    drupal_set_breadcrumb($breadcrumb);
  }


  public function postProcessFacetBuild(array $facet, array &$build) {
    foreach ($build as $value => &$item) {
      $segments = $this->pathSegments;
      $tid = $item['#indexed_value'];

      // Add current facet to path if it is inactive.
      if (!$item['#active']) {
        $segments[$tid] = $this->getPathSegment($facet['field alias'], $tid);
      }
      // Remove current facet from url if it is active.
      else {
        unset($segments[$tid]);
      }

      // Modify paths "vocabulary/term-name-tid".
      // @todo: escape accordingly?
      foreach ($segments as $tid => $segment) {
        $path = '/' . $segment['name'] .
          '/' . $segment['value'] .
          '-' . $tid;

        $item['#path'] .= $path;
      }
    }
  }

}
