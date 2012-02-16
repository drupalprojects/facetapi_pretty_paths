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
    while ($segment['value'] = array_pop($args)) {
      $segment['alias'] = array_pop($args);
      $found = FALSE;

      $this->decodePathSegment($segment);

      // Look for matching facet.
      foreach ($facets as $facet_alias => $facet) {
        $pretty_paths_alias = $this->getFacetPrettyPathsAlias($facet);

        // Add to params if alias from url matches alias from facet settings.
        if ($pretty_paths_alias == $segment['alias']) {
          $found = TRUE;

          // @todo: Make this pluggable?
          // Taxonomy special case: <alias>/<term-name>-<term-id>
          if ($facet['field type'] == 'taxonomy_term') {
            $segment['value'] = array_pop(explode('-', $segment['value']));
          }

          $params[] = rawurlencode($facet_alias) . ':' . $segment['value'];
        }
      }
      // When no more facet path segments have been,
      // we assume the rest of the url as the search basePath.
      if (!$found) {
        array_push($args, $segment['alias'], $segment['value']);
        break;
      }
    }

    // Mock default parameters for facetapi & related search modules.
    if (!empty($params)) {
      // Set search base path.
      $_GET['q'] = implode('/', $args);
      // Set query params.
      $_GET['f'] = $params;
    }
    return $_GET;
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

  public function postProcess($type, array &$args) {
    // @todo: There might be better ways to handle this, plugins?
    switch ($type) {
      case 'facet':
        $this->postProcessFacetBuild($args['facet'], &$args['build']);
        break;

      case 'current_search': {
        $this->postProcessCurrentSearchItem(&$args['variables'], $args['item']);
        break;
      }
    }
  }

  protected function postProcessFacetBuild(array $facet, array &$build) {
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
  protected function postProcessItem(array $facet, array &$item, array &$segments = NULL) {
    if (isset($item['#pretty_paths_processed'])) {
      return;
    }
    // Get all currently active path segments so that they are
    // combined with this facet item.
    $segments = $segments === NULL ? $this->pathSegments : $segments;
    $segment = $this->getPathSegment($facet, $item['#indexed_value']);

    // Remove current facet from url if it is active.
    if ($item['#active']) {
      // Recursively process children, if available.
      // If an active parent is de-selected, all its active children are, too.
      if (!empty($item['#item_children'])) {
        foreach ($item['#item_children'] as &$child) {
          $this->postProcessItem($facet, $child, $segments);
        }
      }
      unset($segments[$segment['key']]);
      $item['#pretty_paths_processed'] = TRUE;
    }

    // Make a local copy as additional segments shouldn't be propagated to parents.
    $mySegments = $segments;
    // Add current facet to path if it is inactive.
    if (!$item['#active']) {
      $mySegments = array_merge($mySegments, array($segment['key'] => $segment));
    }

    // Construct facet path.
    $item['#path'] = $this->constructPath($item['#path'], $mySegments);

    // Remove filter query params.
    unset($item['#query']['f']);
  }

  protected function postProcessCurrentSearchItem(array &$variables, array $item) {
    // Unset current search item.
    $facet_name = $item['facets'][0];
    $facet = $this->adapter->getFacet(array('name' => $facet_name));
    $processor = $this->adapter->getProcessor($facet_name);
    $build = $processor->getBuild();
    $segment = $this->getPathSegment($facet->getFacet(), $item['value']);

    // Construct item path.
    $path = $this->postProcessCurrentSearchItemChild($facet->getFacet(), $build, $item);
    $variables['path'] = $path;

    // Remove filter query params.
    $variables['options']['query']['f'] = array();
  }

  /**
   * Find the item in the hierarchical $build array to process the path correctly.
   */
  protected function postProcessCurrentSearchItemChild(array $facet, array $build, array $item) {
    foreach ($build as $child_value => &$child) {
      if ($item['value'] == $child_value) {
        $this->postProcessItem($facet, &$child);
        return $child['#path'];
      }
      if (!empty($child['#item_children'])) {
        $path = $this->postProcessCurrentSearchItemChild($facet, $child['#item_children'], $item);
        if ($path !== NULL) {
          return $path;
        }
      }
    }
    return NULL;
  }

  /* ### Generic helpers ### */

  protected function constructPath($currentPath, array $segments) {
    // Sort to avoid multiple urls with duplicate content.
    ksort($segments);
    $path = $currentPath;
    foreach ($segments as $key => $segment) {
      $path .=  '/' . $segment['alias'] . '/' . $segment['value'];
    }
    return $path;
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
    $this->encodePathSegment($segment);

    // @todo: Make this pluggable?
    // Taxonomy special case: <alias>/<term-name>-<term-id>
    if ($facet['field type'] == 'taxonomy_term') {
      $term = taxonomy_term_load($value);
      $voc = taxonomy_vocabulary_load($term->vid);
      $segment['value'] = $this->prettyPath($term->name) . '-' . $term->tid;
    }

    $segment['key'] = $pretty_paths_alias . "_" . $value;
    return $segment;
  }

  protected function encodePathSegment(array &$segment) {
    $segment['value'] = str_replace('/', '%2F', $segment['value']);
    $segment['alias'] = rawurlencode($segment['alias']);
  }

  protected function decodePathSegment(array &$segment) {
    $segment['value'] = str_replace('%2F', '/', $segment['value']);
    $segment['alias'] = rawurldecode($segment['alias']);
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
