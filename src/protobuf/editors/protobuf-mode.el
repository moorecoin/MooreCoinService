;;; protobuf-mode.el --- major mode for editing protocol buffers.

;; author: alexandre vassalotti <alexandre@peadrop.com>
;; created: 23-apr-2009
;; version: 0.3
;; keywords: google protobuf languages

;; redistribution and use in source and binary forms, with or without
;; modification, are permitted provided that the following conditions are
;; met:
;;
;;     * redistributions of source code must retain the above copyright
;; notice, this list of conditions and the following disclaimer.
;;     * redistributions in binary form must reproduce the above
;; copyright notice, this list of conditions and the following disclaimer
;; in the documentation and/or other materials provided with the
;; distribution.
;;     * neither the name of google inc. nor the names of its
;; contributors may be used to endorse or promote products derived from
;; this software without specific prior written permission.
;;
;; this software is provided by the copyright holders and contributors
;; "as is" and any express or implied warranties, including, but not
;; limited to, the implied warranties of merchantability and fitness for
;; a particular purpose are disclaimed. in no event shall the copyright
;; owner or contributors be liable for any direct, indirect, incidental,
;; special, exemplary, or consequential damages (including, but not
;; limited to, procurement of substitute goods or services; loss of use,
;; data, or profits; or business interruption) however caused and on any
;; theory of liability, whether in contract, strict liability, or tort
;; (including negligence or otherwise) arising in any way out of the use
;; of this software, even if advised of the possibility of such damage.

;;; commentary:

;; installation:
;;   - put `protobuf-mode.el' in your emacs load-path.
;;   - add this line to your .emacs file:
;;       (require 'protobuf-mode)
;;
;; you can customize this mode just like any mode derived from cc mode.  if
;; you want to add customizations specific to protobuf-mode, you can use the
;; `protobuf-mode-hook'. for example, the following would make protocol-mode
;; use 2-space indentation:
;;
;;   (defconst my-protobuf-style
;;     '((c-basic-offset . 2)
;;       (indent-tabs-mode . nil)))
;;
;;   (add-hook 'protobuf-mode-hook
;;     (lambda () (c-add-style "my-style" my-protobuf-style t)))
;;
;; refer to the documentation of cc mode for more information about
;; customization details and how to use this mode.
;;
;; todo:
;;   - make highlighting for enum values work properly.
;;   - fix the parser to recognize extensions as identifiers and not
;;     as casts.
;;   - improve the parsing of option assignment lists. for example:
;;       optional int32 foo = 1 [(my_field_option) = 4.5];
;;   - add support for fully-qualified identifiers (e.g., with a leading ".").

;;; code:

(require 'cc-mode)

(eval-when-compile
  (require 'cc-langs)
  (require 'cc-fonts))

;; this mode does not inherit properties from other modes. so, we do not use 
;; the usual `c-add-language' function.
(eval-and-compile
  (put 'protobuf-mode 'c-mode-prefix "protobuf-"))

;; the following code uses of the `c-lang-defconst' macro define syntactic
;; features of protocol buffer language.  refer to the documentation in the
;; cc-langs.el file for information about the meaning of the -kwds variables.

(c-lang-defconst c-primitive-type-kwds
  protobuf '("double" "float" "int32" "int64" "uint32" "uint64" "sint32"
             "sint64" "fixed32" "fixed64" "sfixed32" "sfixed64" "bool"
             "string" "bytes" "group"))

(c-lang-defconst c-modifier-kwds
  protobuf '("required" "optional" "repeated"))

(c-lang-defconst c-class-decl-kwds
  protobuf '("message" "enum" "service"))

(c-lang-defconst c-constant-kwds
  protobuf '("true" "false"))

(c-lang-defconst c-other-decl-kwds
  protobuf '("package" "import"))

(c-lang-defconst c-other-kwds
  protobuf '("default" "max"))

(c-lang-defconst c-identifier-ops
  ;; handle extended identifiers like google.protobuf.messageoptions
  protobuf '((left-assoc ".")))

;; the following keywords do not fit well in keyword classes defined by
;; cc-mode.  so, we approximate as best we can.

(c-lang-defconst c-type-list-kwds
  protobuf '("extensions" "to"))

(c-lang-defconst c-typeless-decl-kwds
  protobuf '("extend" "rpc" "option" "returns"))


;; here we remove default syntax for loops, if-statements and other c
;; syntactic features that are not supported by the protocol buffer language.

(c-lang-defconst c-brace-list-decl-kwds
  ;; remove syntax for c-style enumerations.
  protobuf nil)

(c-lang-defconst c-block-stmt-1-kwds
  ;; remove syntax for "do" and "else" keywords.
  protobuf nil)

(c-lang-defconst c-block-stmt-2-kwds
  ;; remove syntax for "for", "if", "switch" and "while" keywords.
  protobuf nil)

(c-lang-defconst c-simple-stmt-kwds
  ;; remove syntax for "break", "continue", "goto" and "return" keywords.
  protobuf nil)

(c-lang-defconst c-paren-stmt-kwds
  ;; remove special case for the "(;;)" in for-loops.
  protobuf nil)

(c-lang-defconst c-label-kwds
  ;; remove case label syntax for the "case" and "default" keywords.
  protobuf nil)

(c-lang-defconst c-before-label-kwds
  ;; remove special case for the label in a goto statement.
  protobuf nil)

(c-lang-defconst c-cpp-matchers
  ;; disable all the c preprocessor syntax.
  protobuf nil)

(c-lang-defconst c-decl-prefix-re
  ;; same as for c, except it does not match "(". this is needed for disabling
  ;; the syntax for casts.
  protobuf "\\([\{\};,]+\\)")


;; add support for variable levels of syntax highlighting.

(defconst protobuf-font-lock-keywords-1 (c-lang-const c-matchers-1 protobuf)
  "minimal highlighting for protobuf-mode.")

(defconst protobuf-font-lock-keywords-2 (c-lang-const c-matchers-2 protobuf)
  "fast normal highlighting for protobuf-mode.")

(defconst protobuf-font-lock-keywords-3 (c-lang-const c-matchers-3 protobuf)
  "accurate normal highlighting for protobuf-mode.")

(defvar protobuf-font-lock-keywords protobuf-font-lock-keywords-3
  "default expressions to highlight in protobuf-mode.")

;; our syntax table is auto-generated from the keyword classes we defined
;; previously with the `c-lang-const' macro.
(defvar protobuf-mode-syntax-table nil
  "syntax table used in protobuf-mode buffers.")
(or protobuf-mode-syntax-table
    (setq protobuf-mode-syntax-table
          (funcall (c-lang-const c-make-mode-syntax-table protobuf))))

(defvar protobuf-mode-abbrev-table nil
  "abbreviation table used in protobuf-mode buffers.")

(defvar protobuf-mode-map nil
  "keymap used in protobuf-mode buffers.")
(or protobuf-mode-map
    (setq protobuf-mode-map (c-make-inherited-keymap)))

(easy-menu-define protobuf-menu protobuf-mode-map
  "protocol buffers mode commands"
  (cons "protocol buffers" (c-lang-const c-mode-menu protobuf)))

;;;###autoload (add-to-list 'auto-mode-alist '("\\.proto\\'" . protobuf-mode))

;;;###autoload
(defun protobuf-mode ()
  "major mode for editing protocol buffers description language.

the hook `c-mode-common-hook' is run with no argument at mode
initialization, then `protobuf-mode-hook'.

key bindings:
\\{protobuf-mode-map}"
  (interactive)
  (kill-all-local-variables)
  (set-syntax-table protobuf-mode-syntax-table)
  (setq major-mode 'protobuf-mode
        mode-name "protocol-buffers"
        local-abbrev-table protobuf-mode-abbrev-table
        abbrev-mode t)
  (use-local-map protobuf-mode-map)
  (c-initialize-cc-mode t)
  (if (fboundp 'c-make-emacs-variables-local)
      (c-make-emacs-variables-local))
  (c-init-language-vars protobuf-mode)
  (c-common-init 'protobuf-mode)
  (easy-menu-add protobuf-menu)
  (c-run-mode-hooks 'c-mode-common-hook 'protobuf-mode-hook)
  (c-update-modeline))

(provide 'protobuf-mode)

;;; protobuf-mode.el ends here
