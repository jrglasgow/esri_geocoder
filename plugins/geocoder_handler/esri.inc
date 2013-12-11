<?php


/**
 * @file
 * Plugin to provide an ESRI geocoder.
 */

/**
 * Plugins are described by creating a $plugin array which will be used
 * by the system that includes this file.
 */
$plugin = array(
  'title' => t("ESRI Geocoder"),
  'description' => t('Geocodes via ESRI geocoder'),
  'callback' => 'geocoder_esri',
  'field_types' => array('text', 'text_long', 'addressfield', 'location', 'text_with_summary', 'computed', 'taxonomy_term_reference'),
  'field_callback' => 'geocoder_esri_field',
  'settings_callback' => 'geocoder_esri_form',
  'terms_of_service' => 'http://blogs.esri.com/esri/arcgis/2013/02/21/terms-of-use-for-batch-geocoding/',
);

/**
 * Process Markup
 */
function geocoder_esri($address, $options = array()) {
  try {
    geophp_load();

    $query = array(
      'text'  => $address,
      'f'     => 'pjson',
    );

    $url = url("http://geocode.arcgis.com/arcgis/rest/services/World/GeocodeServer/find", array('query' => $query));
    $result = drupal_http_request($url);

    if (isset($result->error)) {
      $args = array(
        '@code' => $result->code,
        '@error' => $result->error,
      );
      $msg = t('HTTP request to google API failed.\nCode: @code\nError: @error', $args);
      throw new Exception($msg);
    }

    $data = json_decode($result->data);

    $geometries = array();
    $geom = new Point($data->locations[0]->feature->geometry->x, $data->locations[0]->feature->geometry->y);
    $geometries[] = $geom;

    if (empty($geometries)) {
      return;
    }

    // Check if we should return all results as a compound geometry
    if (isset($options['all_results'])) {
      if ($options['all_results']) {
        return geoPHP::geometryReduce($geometries);
      }
    }
    // The connonical geometry is the first result (best guesse)
    $geometry = array_shift($geometries);

    // If there are any other geometries, these are auxiliary geometries that represent "alternatives"
    if (count($geometries)) {
      $geometry->data['geocoder_alternatives'] = $geometries;
    }

    return $geometry;
  } catch (Exception $e) {
    watchdog_exception('geocoder', $e);
    return FALSE;
  }
}

function geocoder_esri_field($field, $field_item, $options = array()) {
  if ($field['type'] == 'text' || $field['type'] == 'text_long' || $field['type'] == 'text_with_summary' || $field['type'] == 'computed') {
    return geocoder_esri($field_item['value'], $options);
  }
  if ($field['type'] == 'addressfield') {
    $address = geocoder_widget_parse_addressfield($field_item);
    return geocoder_esri($address, $options);
  }
  if ($field['type'] == 'location') {
    $address = geocoder_widget_parse_locationfield($field_item);
    return geocoder_esri($address, $options);
  }
  if ($field['type'] == 'taxonomy_term_reference') {
    $term = taxonomy_term_load($field_item['tid']);
    return geocoder_esri($term->name);
  }
}

function geocoder_esri_form($default_values = array()) {
  $form = array();

  $form['geometry_type'] = array(
    '#type' => 'select',
    '#title' => 'Geometry Type',
    '#options' => array(
      'point' => 'Point (default)',
      'bounds' => 'Bounding Box',
      'viewport' => 'Viewport',
    ),
    '#default_value' => isset($default_values['geometry_type']) ? $default_values['geometry_type'] : 'point',
  );

  $form['all_results'] = array(
    '#type' => 'checkbox',
    '#title' => 'Geocode all alternative results',
    '#default_value' => isset($default_values['all_results']) ? $default_values['all_results'] : FALSE,
    '#description' => 'Often an ambiguous address (such as "Springfield USA") can result in multiple hits. By default we only return the first (best guess) result. Check this to return all results as a Multi-Geometry (MultiPoint or MultiPolygon).',
  );

  $form['reject_results'] = array(
    '#type' => 'checkboxes',
    '#title' => 'Reject Results',
    '#options' => array(
      'APPROXIMATE' => 'APPROXIMATE:  indicates that the returned result is approximate.',
      'GEOMETRIC_CENTER'   => 'GEOMETRIC_CENTER: indicates that the returned result is the geometric center of a result such as a polyline (for example, a street) or polygon (region).',
      'RANGE_INTERPOLATED' => 'RANGE_INTERPOLATED: indicates that the returned result reflects an approximation (usually on a road) interpolated between two precise points (such as intersections). Interpolated results are generally returned when rooftop geocodes are unavailable for a street address.',
      'ROOFTOP' => 'ROOFTOP: indicates that the returned result is a precise geocode for which we have location information accurate down to street address precision.',
    ),
    '#default_value' => isset($default_values['reject_results']) ? $default_values['reject_results'] : array(),
    '#description' => 'Reject results that do not meet a certain level of quality or precision. Check all types of results to reject.',
  );
  return array();
  return $form;
}
