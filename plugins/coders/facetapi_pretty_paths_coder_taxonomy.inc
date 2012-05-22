<?php

/**
 * @file
 * A taxonomy specific coder for pretty paths.
 */

/**
 * Taxonomy specific implementation of FacetApiPrettyPathsCoder.
 */
class FacetApiPrettyPathsCoderTaxonomy extends FacetApiPrettyPathsCoderDefault {

  public function encodePathSegment(array $args) {
    // Taxonomy special case: <alias>/<term-name>-<term-id>
    if ($term = taxonomy_term_load($args['segment']['value'])) {
      $args['segment']['value'] = $this->prettyPath($term->name) . '-' . $term->tid;
    }
    return parent::encodePathSegment($args);
  }

  public function decodePathSegmentValue(array $args) {
    // Taxonomy special case: <alias>/<term-name>-<term-id>
    $args['value'] = array_pop(explode('-', $args['value']));
    return parent::decodePathSegmentValue($args);
  }

}