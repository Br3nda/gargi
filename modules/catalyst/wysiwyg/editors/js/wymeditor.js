// $Id: wymeditor.js,v 1.2 2009/06/06 02:18:36 sun Exp $

/**
 * Attach this editor to a target element.
 */
Drupal.wysiwyg.editor.attach.wymeditor = function(context, params, settings) {
  // Prepend basePath to wymPath.
  settings.wymPath = settings.basePath + settings.wymPath;
  // Attach editor.
  $('#' + params.field).wymeditor(settings);
};

/**
 * Detach a single or all editors.
 */
Drupal.wysiwyg.editor.detach.wymeditor = function(context, params) {
  if (typeof params != 'undefined') {
    var $field = $('#' + params.field);
    var index = $field.data(WYMeditor.WYM_INDEX);
    if (typeof index != 'undefined') {
      var instance = WYMeditor.INSTANCES[index];
      instance.update();
      $(instance._box).remove();
      $(instance._element).show();
      delete instance;
    }
    $field.show();
  }
  else {
    jQuery.each(WYMeditor.INSTANCES, function() {
      this.update();
      $(this._box).remove();
      $(this._element).show();
      delete this;
    });
  }
};

