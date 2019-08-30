import { Injectable } from '@angular/core';

import { I18n } from '@ngx-translate/i18n-polyfill';
import * as _ from 'lodash';

@Injectable({
  providedIn: 'root'
})
export class UserChangePasswordService {
  requiredPasswordRulesMessage: string;
  passwordStrengthLevel: string;
  passwordStrengthDescription: string;

  constructor(private i18n: I18n) {}
  getPasswordRulesMessage() {
    return this.i18n(
      'Required  rules for password complexity:\n\
  - cannot contain username\n\
  - cannot contain any keyword used in Ceph\n\
  - must  consist of characters from the following groups:\n\
    * alphabetic a-z, A-Z\n\
    * numbers 0-9\n\
    * special chars: !"#$%& \'()*+,-./:;<=>?@[\\]^_`{{|}}~\n\
    * any other characters (signs)'
    );
  }

  checkPasswordComplexity(password): [string, string] {
    this.passwordStrengthLevel = 'passwordStrengthLevel0';
    this.passwordStrengthDescription = '';
    const credits = this.checkPasswordComplexityLetters(password);
    if (credits) {
      if (credits < 10) {
        this.passwordStrengthLevel = 'passwordStrengthLevel0';
        this.passwordStrengthDescription = this.i18n('Too weak');
      } else {
        if (credits < 15) {
          this.passwordStrengthLevel = 'passwordStrengthLevel1';
          this.passwordStrengthDescription = this.i18n('Weak');
        } else {
          if (credits < 20) {
            this.passwordStrengthLevel = 'passwordStrengthLevel2';
            this.passwordStrengthDescription = this.i18n('OK');
          } else {
            if (credits < 25) {
              this.passwordStrengthLevel = 'passwordStrengthLevel3';
              this.passwordStrengthDescription = this.i18n('Strong');
            } else {
              this.passwordStrengthLevel = 'passwordStrengthLevel4';
              this.passwordStrengthDescription = this.i18n('Very strong');
            }
          }
        }
      }
    }
    return [this.passwordStrengthLevel, this.passwordStrengthDescription];
  }

  private checkPasswordComplexityLetters(password): number {
    if (_.isString(password)) {
      const digitsNumber = password.replace(/[^0-9]/g, '').length;
      const smallLettersNumber = password.replace(/[^a-z]/g, '').length;
      const bigLettersNumber = password.replace(/[^A-Z]/g, '').length;
      const punctuationNumber = password.replace(/[^!"#$%&'()*+,-./:;<=>?@[\]^_`{|}~]/g, '').length;
      const othersCharactersNumber =
        password.length -
        (digitsNumber + smallLettersNumber + bigLettersNumber + punctuationNumber);
      return (
        digitsNumber +
        smallLettersNumber +
        bigLettersNumber * 2 +
        punctuationNumber * 3 +
        othersCharactersNumber * 5
      );
    } else {
      return 0;
    }
  }
}
